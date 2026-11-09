#include "spacetimedb.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

using namespace SpacetimeDB;

// Unreal world units: centimeters, and degrees for the rotator.
struct DbVector3
{
    float x;
    float y;
    float z;
};
SPACETIMEDB_STRUCT(DbVector3, x, y, z)

struct DbRotator
{
    float pitch;
    float yaw;
    float roll;
};
SPACETIMEDB_STRUCT(DbRotator, pitch, yaw, roll)

// Private. Clients must never subscribe to this.
struct Account
{
    std::string name;
    std::string password;
};
SPACETIMEDB_STRUCT(Account, name, password)
SPACETIMEDB_TABLE(Account, account, Private)
FIELD_PrimaryKey(account, name)

// Public. One persistent row per username. Never delete it on logout.
// identity is a stable player key assigned once at account creation. It is
// not the caller's connection identity, so two clients that share a Spacetime
// token still control different rows.
struct Player
{
    std::string name;
    Identity identity;
    DbVector3 position;
    DbRotator rotation;
    bool online;
    Timestamp updated_at;
    // 0 = Hero, 1 = Explorer. Chosen once when the row is created.
    uint8_t role;
};
SPACETIMEDB_STRUCT(Player, name, identity, position, rotation, online, updated_at, role)
SPACETIMEDB_TABLE(Player, player, Public)
FIELD_PrimaryKey(player, name)
FIELD_Unique(player, identity)
FIELD_Default(player, role, uint8_t{0})

// Private. Which live connection may move which username.
struct PlayerSession
{
    ConnectionId connection_id;
    std::string name;
};
SPACETIMEDB_STRUCT(PlayerSession, connection_id, name)
SPACETIMEDB_TABLE(PlayerSession, player_session, Private)
FIELD_PrimaryKey(player_session, connection_id)
FIELD_Unique(player_session, name)

// Public. One persistent row per monster. Clients spawn from this table.
// kind: 0 = Chest Monster, 1 = Ghost Fish.
// spawn_* is the authored point and is never overwritten by movement updates.
// owner is the player name currently driving the monster, or empty when unclaimed.
// loop_attack is true only while that owner still sees a player in range.
struct Monster
{
    uint64_t id;
    uint8_t kind;
    DbVector3 spawn_position;
    DbRotator spawn_rotation;
    DbVector3 position;
    DbRotator rotation;
    std::string owner;
    bool is_attacking;
    bool loop_attack;
    Timestamp updated_at;
};
SPACETIMEDB_STRUCT(
    Monster,
    id,
    kind,
    spawn_position,
    spawn_rotation,
    position,
    rotation,
    owner,
    is_attacking,
    loop_attack,
    updated_at)
SPACETIMEDB_TABLE(Monster, monster, Public)
FIELD_PrimaryKeyAutoInc(monster, id)
FIELD_Index(monster, owner)

namespace
{
constexpr size_t kMaxUsernameLength = 24;

bool IsValidUsername(const std::string& Name)
{
    if (Name.empty() || Name.size() > kMaxUsernameLength)
    {
        return false;
    }

    for (const char Character : Name)
    {
        const bool IsLetter =
            (Character >= 'a' && Character <= 'z') || (Character >= 'A' && Character <= 'Z');
        const bool IsDigit = Character >= '0' && Character <= '9';
        if (!IsLetter && !IsDigit)
        {
            return false;
        }
    }

    return true;
}

bool PasswordMatches(const Account& AccountRow, const std::string& Password)
{
    return AccountRow.password == Password;
}

bool IsZeroIdentity(const Identity& Key)
{
    const Identity::ByteArray& Bytes = Key.get_bytes();
    return std::all_of(Bytes.begin(), Bytes.end(), [](uint8_t Byte) { return Byte == 0; });
}

Identity AllocatePlayerIdentity(ReducerContext& ctx)
{
    std::array<uint8_t, Identity::SIZE> Bytes{};
    for (;;)
    {
        ctx.rng().fill_bytes(Bytes.data(), Bytes.size());
        if (std::all_of(Bytes.begin(), Bytes.end(), [](uint8_t Byte) { return Byte == 0; }))
        {
            continue;
        }

        const Identity Key(Bytes);
        if (!ctx.db[player_identity].find(Key))
        {
            return Key;
        }
    }
}

void ReleaseMonstersOwnedBy(ReducerContext& ctx, const std::string& Name);

void BindConnection(ReducerContext& ctx, const ConnectionId& Connection, const std::string& Name)
{
    if (auto ExistingByName = ctx.db[player_session_name].find(Name))
    {
        if (ExistingByName->connection_id != Connection)
        {
            ctx.db[player_session_connection_id].delete_by_key(ExistingByName->connection_id);
        }
    }

    if (auto ExistingByConnection = ctx.db[player_session_connection_id].find(Connection))
    {
        if (ExistingByConnection->name != Name)
        {
            ReleaseMonstersOwnedBy(ctx, ExistingByConnection->name);
            if (auto OtherPlayer = ctx.db[player_name].find(ExistingByConnection->name))
            {
                OtherPlayer->online = false;
                OtherPlayer->updated_at = ctx.timestamp;
                ctx.db[player_name].update(*OtherPlayer);
            }

            ExistingByConnection->name = Name;
            ctx.db[player_session_connection_id].update(*ExistingByConnection);
        }
        return;
    }

    ctx.db[player_session].insert(PlayerSession{Connection, Name});
}

void ReleaseMonstersOwnedBy(ReducerContext& ctx, const std::string& Name)
{
    if (Name.empty())
    {
        return;
    }

    std::vector<Monster> Owned;
    for (const auto& Row : ctx.db[monster_owner].filter(Name))
    {
        Owned.push_back(Row);
    }

    for (Monster& Row : Owned)
    {
        Row.owner.clear();
        Row.is_attacking = false;
        Row.loop_attack = false;
        Row.updated_at = ctx.timestamp;
        ctx.db[monster_id].update(Row);
    }
}

void SetPlayerOfflineByConnection(ReducerContext& ctx)
{
    if (!ctx.connection_id)
    {
        return;
    }

    const ConnectionId Connection = *ctx.connection_id;
    if (auto Session = ctx.db[player_session_connection_id].find(Connection))
    {
        ReleaseMonstersOwnedBy(ctx, Session->name);

        if (auto PlayerRow = ctx.db[player_name].find(Session->name))
        {
            PlayerRow->online = false;
            PlayerRow->updated_at = ctx.timestamp;
            ctx.db[player_name].update(*PlayerRow);
        }

        ctx.db[player_session_connection_id].delete_by_key(Connection);
    }
}

bool UpdateOnlinePlayerTransform(
    ReducerContext& ctx,
    const DbVector3& Position,
    const DbRotator& Rotation)
{
    if (!ctx.connection_id)
    {
        return false;
    }

    const auto Session = ctx.db[player_session_connection_id].find(*ctx.connection_id);
    if (!Session)
    {
        return false;
    }

    auto PlayerRow = ctx.db[player_name].find(Session->name);
    if (!PlayerRow || !PlayerRow->online)
    {
        return false;
    }

    PlayerRow->position = Position;
    PlayerRow->rotation = Rotation;
    PlayerRow->updated_at = ctx.timestamp;
    ctx.db[player_name].update(*PlayerRow);
    return true;
}

// Logged-in player name for this connection, or empty when the caller cannot act.
std::string OnlineCallerName(ReducerContext& ctx)
{
    if (!ctx.connection_id)
    {
        return {};
    }

    const auto Session = ctx.db[player_session_connection_id].find(*ctx.connection_id);
    if (!Session)
    {
        return {};
    }

    const auto PlayerRow = ctx.db[player_name].find(Session->name);
    if (!PlayerRow || !PlayerRow->online)
    {
        return {};
    }

    return Session->name;
}

bool MonsterOwnerIsAbsent(ReducerContext& ctx, const std::string& Owner)
{
    if (Owner.empty())
    {
        return true;
    }

    const auto OwnerRow = ctx.db[player_name].find(Owner);
    return !OwnerRow || !OwnerRow->online;
}

Monster MakeMonster(uint8_t Kind, const DbVector3& Spawn, const DbRotator& Rotation, Timestamp UpdatedAt)
{
    return Monster{
        0,
        Kind,
        Spawn,
        Rotation,
        Spawn,
        Rotation,
        std::string{},
        false,
        false,
        UpdatedAt
    };
}

// Positions match the actors that were placed in Lvl_Main before server seeding.
void SeedMonstersIfEmpty(ReducerContext& ctx)
{
    if (ctx.db[monster].count() > 0)
    {
        return;
    }

    const DbRotator Rotation{0.f, 0.f, 0.f};
    const DbVector3 ChestSpawn{-30.f, 420.f, 70.f};
    const DbVector3 GhostFishSpawn{-510.f, 440.f, 70.f};
    ctx.db[monster].insert(MakeMonster(0, ChestSpawn, Rotation, ctx.timestamp));
    ctx.db[monster].insert(MakeMonster(1, GhostFishSpawn, Rotation, ctx.timestamp));
    LOG_INFO("Seeded Chest Monster and Ghost Fish");
}
} // namespace

SPACETIMEDB_INIT(init, ReducerContext ctx)
{
    LOG_INFO("TheGoldenAge module ready");
    SeedMonstersIfEmpty(ctx);
    return Ok();
}

// init does not re-run on an existing database. Call this once after publish.
SPACETIMEDB_REDUCER(seed_monsters, ReducerContext ctx)
{
    SeedMonstersIfEmpty(ctx);
    return Ok();
}

SPACETIMEDB_CLIENT_CONNECTED(on_connect, ReducerContext ctx)
{
    LOG_INFO("Client connected: " + ctx.sender().to_string());
    return Ok();
}

SPACETIMEDB_CLIENT_DISCONNECTED(on_disconnect, ReducerContext ctx)
{
    SetPlayerOfflineByConnection(ctx);
    return Ok();
}

SPACETIMEDB_REDUCER(login, ReducerContext ctx, std::string name, std::string password, uint8_t role)
{
    if (!IsValidUsername(name))
    {
        return Err("Username must be 1-24 letters and numbers only");
    }

    if (password.empty())
    {
        return Err("Password cannot be empty");
    }

    if (!ctx.connection_id)
    {
        return Err("Missing connection");
    }

    const ConnectionId connection = *ctx.connection_id;

    if (auto account = ctx.db[account_name].find(name))
    {
        if (!PasswordMatches(*account, password))
        {
            return Err("Incorrect password");
        }

        auto player = ctx.db[player_name].find(name);
        if (!player)
        {
            return Err("Account exists but player row is missing");
        }

        if (auto session = ctx.db[player_session_name].find(name))
        {
            if (session->connection_id != connection)
            {
                return Err("Already online");
            }
        }

        if (IsZeroIdentity(player->identity))
        {
            player->identity = AllocatePlayerIdentity(ctx);
        }

        BindConnection(ctx, connection, name);
        player->online = true;
        player->updated_at = ctx.timestamp;
        ctx.db[player_name].update(*player);
        return Ok();
    }

    if (role > 1)
    {
        return Err("Choose Hero or Explorer");
    }

    const Identity player_identity_key = AllocatePlayerIdentity(ctx);
    BindConnection(ctx, connection, name);
    ctx.db[account].insert(Account{name, password});
    ctx.db[player].insert(Player{
        name,
        player_identity_key,
        DbVector3{0.f, 0.f, 0.f},
        DbRotator{0.f, 0.f, 0.f},
        true,
        ctx.timestamp,
        role
    });
    return Ok();
}

// The caller is resolved only through the session for ctx.connection_id.
SPACETIMEDB_REDUCER(update_player_transform, ReducerContext ctx, DbVector3 position, DbRotator rotation)
{
    if (!UpdateOnlinePlayerTransform(ctx, position, rotation))
    {
        return Err("Not logged in");
    }

    return Ok();
}

SPACETIMEDB_REDUCER(claim_monster, ReducerContext ctx, uint64_t id)
{
    const std::string Caller = OnlineCallerName(ctx);
    if (Caller.empty())
    {
        return Err("Not logged in");
    }

    auto Row = ctx.db[monster_id].find(id);
    if (!Row)
    {
        return Err("Unknown monster");
    }

    if (Row->owner == Caller)
    {
        return Ok();
    }

    if (!MonsterOwnerIsAbsent(ctx, Row->owner))
    {
        return Err("Already claimed");
    }

    Row->owner = Caller;
    Row->updated_at = ctx.timestamp;
    ctx.db[monster_id].update(*Row);
    return Ok();
}

SPACETIMEDB_REDUCER(
    update_monster,
    ReducerContext ctx,
    uint64_t id,
    DbVector3 position,
    DbRotator rotation,
    bool is_attacking,
    bool loop_attack)
{
    const std::string Caller = OnlineCallerName(ctx);
    if (Caller.empty())
    {
        return Err("Not logged in");
    }

    auto Row = ctx.db[monster_id].find(id);
    if (!Row)
    {
        return Err("Unknown monster");
    }

    if (Row->owner != Caller)
    {
        return Err("Not the owner");
    }

    Row->position = position;
    Row->rotation = rotation;
    Row->is_attacking = is_attacking;
    Row->loop_attack = loop_attack;
    Row->updated_at = ctx.timestamp;
    ctx.db[monster_id].update(*Row);
    return Ok();
}

SPACETIMEDB_REDUCER(release_monster, ReducerContext ctx, uint64_t id)
{
    const std::string Caller = OnlineCallerName(ctx);
    if (Caller.empty())
    {
        return Err("Not logged in");
    }

    auto Row = ctx.db[monster_id].find(id);
    if (!Row)
    {
        return Err("Unknown monster");
    }

    if (Row->owner != Caller)
    {
        return Err("Not the owner");
    }

    Row->owner.clear();
    Row->is_attacking = false;
    Row->loop_attack = false;
    Row->updated_at = ctx.timestamp;
    ctx.db[monster_id].update(*Row);
    return Ok();
}
