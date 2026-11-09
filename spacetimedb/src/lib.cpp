#include "spacetimedb.h"

#include <algorithm>
#include <array>
#include <string>

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
};
SPACETIMEDB_STRUCT(Player, name, identity, position, rotation, online, updated_at)
SPACETIMEDB_TABLE(Player, player, Public)
FIELD_PrimaryKey(player, name)
FIELD_Unique(player, identity)

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

void SetPlayerOfflineByConnection(ReducerContext& ctx)
{
    if (!ctx.connection_id)
    {
        return;
    }

    const ConnectionId Connection = *ctx.connection_id;
    if (auto Session = ctx.db[player_session_connection_id].find(Connection))
    {
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
} // namespace

SPACETIMEDB_INIT(init, ReducerContext ctx)
{
    LOG_INFO("TheGoldenAge module ready");
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

SPACETIMEDB_REDUCER(login, ReducerContext ctx, std::string name, std::string password)
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

    const Identity player_identity_key = AllocatePlayerIdentity(ctx);
    BindConnection(ctx, connection, name);
    ctx.db[account].insert(Account{name, password});
    ctx.db[player].insert(Player{
        name,
        player_identity_key,
        DbVector3{0.f, 0.f, 0.f},
        DbRotator{0.f, 0.f, 0.f},
        true,
        ctx.timestamp
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
