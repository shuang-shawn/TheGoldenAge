#include "spacetimedb.h"

#include <algorithm>
#include <array>
#include <cmath>
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
    // Server-owned hit points. Clients render this value. 0 means dead.
    float health;
    // True while the owning client is playing a one-shot attack (e.g. Hero Attack 01).
    bool is_attacking;
};
SPACETIMEDB_STRUCT(Player, name, identity, position, rotation, online, updated_at, role, health, is_attacking)
SPACETIMEDB_TABLE(Player, player, Public)
FIELD_PrimaryKey(player, name)
FIELD_Unique(player, identity)
FIELD_Default(player, role, uint8_t{0})
FIELD_Default(player, health, 100.f)
FIELD_Default(player, is_attacking, false)

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

// Public. One persistent row per monster. Clients spawn from this table and only render it.
// kind: 0 = Chest Monster, 1 = Ghost Fish.
// spawn_* is the authored point and is never overwritten by movement updates.
// The module writes position, rotation, and the attack flags. No client owns a row.
// loop_attack is true only while a player is inside attack_range.
struct Monster
{
    uint64_t id;
    uint8_t kind;
    DbVector3 spawn_position;
    DbRotator spawn_rotation;
    DbVector3 position;
    DbRotator rotation;
    bool is_attacking;
    bool loop_attack;
    // Server-owned hit points. Clients render this value. 0 means dead.
    float health;
    Timestamp updated_at;
    // Horizontal detection and attack reach in centimeters. Zero uses the kind seed default.
    float detection_range;
    float attack_range;
    // Hit points removed from a player on a validated attack hit. Zero uses kDefaultMonsterAttackDamage.
    float attack_damage;
};
SPACETIMEDB_STRUCT(
    Monster,
    id,
    kind,
    spawn_position,
    spawn_rotation,
    position,
    rotation,
    is_attacking,
    loop_attack,
    health,
    updated_at,
    detection_range,
    attack_range,
    attack_damage)
SPACETIMEDB_TABLE(Monster, monster, Public)
FIELD_PrimaryKeyAutoInc(monster, id)
FIELD_Default(monster, health, 100.f)
FIELD_Default(monster, detection_range, 0.f)
FIELD_Default(monster, attack_range, 0.f)
FIELD_Default(monster, attack_damage, 15.f)

// Private. Debounces monster_attack_player from overlapping clients.
struct MonsterAttackHitCooldown
{
    std::string cooldown_key;
    Timestamp last_hit_at;
};
SPACETIMEDB_STRUCT(MonsterAttackHitCooldown, cooldown_key, last_hit_at)
SPACETIMEDB_TABLE(MonsterAttackHitCooldown, monster_attack_hit_cooldown, Private)
FIELD_PrimaryKey(monster_attack_hit_cooldown, cooldown_key)

// Private. One interval row keeps monster_tick running. Clients never subscribe to it.
struct MonsterTickTimer
{
    uint64_t scheduled_id;
    ScheduleAt scheduled_at;
};
SPACETIMEDB_STRUCT(MonsterTickTimer, scheduled_id, scheduled_at)
SPACETIMEDB_TABLE(MonsterTickTimer, monster_tick_timer, Private)
FIELD_PrimaryKeyAutoInc(monster_tick_timer, scheduled_id)
SPACETIMEDB_SCHEDULE(monster_tick_timer, 1, monster_tick)

// Private. One shot, one second after health reaches 0. The reducer deletes the row when it runs.
struct PlayerRespawnTimer
{
    uint64_t scheduled_id;
    ScheduleAt scheduled_at;
    std::string name;
};
SPACETIMEDB_STRUCT(PlayerRespawnTimer, scheduled_id, scheduled_at, name)
SPACETIMEDB_TABLE(PlayerRespawnTimer, player_respawn_timer, Private)
FIELD_PrimaryKeyAutoInc(player_respawn_timer, scheduled_id)
SPACETIMEDB_SCHEDULE(player_respawn_timer, 1, respawn_player)

// Private. One shot, one second after a monster's health reaches 0.
struct MonsterRespawnTimer
{
    uint64_t scheduled_id;
    ScheduleAt scheduled_at;
    uint64_t monster_id;
};
SPACETIMEDB_STRUCT(MonsterRespawnTimer, scheduled_id, scheduled_at, monster_id)
SPACETIMEDB_TABLE(MonsterRespawnTimer, monster_respawn_timer, Private)
FIELD_PrimaryKeyAutoInc(monster_respawn_timer, scheduled_id)
SPACETIMEDB_SCHEDULE(monster_respawn_timer, 1, respawn_monster)

namespace
{
constexpr size_t kMaxUsernameLength = 24;
constexpr float kPlayerMaxHealth = 100.f;
constexpr float kMonsterMaxHealth = 100.f;
constexpr float kDefaultMonsterAttackDamage = 15.f;
constexpr float kMonsterHitRangeSlack = 100.f;
constexpr int64_t kMonsterAttackHitCooldownMillis = 400;
constexpr int64_t kPlayerRespawnDelayMillis = 1000;
constexpr int64_t kMonsterRespawnDelayMillis = 1000;

void SchedulePlayerRespawn(ReducerContext& ctx, const Player& PlayerRow);

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
    const DbRotator& Rotation,
    bool IsAttacking)
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

    // A dead body stays where it fell. The respawn reducer writes the next transform.
    if (PlayerRow->health <= 0.f)
    {
        return true;
    }

    PlayerRow->position = Position;
    PlayerRow->rotation = Rotation;
    PlayerRow->is_attacking = IsAttacking;
    PlayerRow->updated_at = ctx.timestamp;
    ctx.db[player_name].update(*PlayerRow);
    return true;
}

// One scheduled reducer for every monster. 50 ms is 20 Hz, which is enough for
// client interpolation. Raise this only if the chase looks coarse.
constexpr int64_t kMonsterTickMillis = 50;
constexpr float kMonsterTickSeconds = 0.05f;
constexpr float kMonsterStateEpsilon = 0.01f;
constexpr float kDegreesPerRadian = 57.2957795f;

struct MonsterKindConfig
{
    float DetectionRange;
    float AttackRange;
    float StopDistance;
    float Speed;
    float TurnRate;
};

// Kind defaults for seeding and for rows that leave a range at zero.
MonsterKindConfig ConfigForKind(uint8_t Kind)
{
    if (Kind == 0)
    {
        // Chest Monster. It turns and attacks, and it does not move.
        return MonsterKindConfig{500.f, 500.f, 0.f, 0.f, 480.f};
    }

    // Ghost Fish. Detection 250, attack and stop at 120, chase at 250 cm/s.
    return MonsterKindConfig{250.f, 120.f, 120.f, 250.f, 480.f};
}

float EffectiveDetectionRange(const Monster& Row)
{
    if (Row.detection_range > 0.f)
    {
        return Row.detection_range;
    }

    return ConfigForKind(Row.kind).DetectionRange;
}

float EffectiveAttackRange(const Monster& Row)
{
    if (Row.attack_range > 0.f)
    {
        return Row.attack_range;
    }

    return ConfigForKind(Row.kind).AttackRange;
}

float EffectiveAttackDamage(const Monster& Row)
{
    if (Row.attack_damage > 0.f)
    {
        return Row.attack_damage;
    }

    return kDefaultMonsterAttackDamage;
}

Monster MakeMonster(uint8_t Kind, const DbVector3& Spawn, const DbRotator& Rotation, Timestamp UpdatedAt)
{
    const MonsterKindConfig Defaults = ConfigForKind(Kind);
    return Monster{
        0,
        Kind,
        Spawn,
        Rotation,
        Spawn,
        Rotation,
        false,
        false,
        kMonsterMaxHealth,
        UpdatedAt,
        Defaults.DetectionRange,
        Defaults.AttackRange,
        kDefaultMonsterAttackDamage
    };
}

std::string MonsterHitCooldownKey(uint64_t MonsterId, const std::string& PlayerName)
{
    return std::to_string(MonsterId) + ":" + PlayerName;
}

bool IsMonsterAttackHitOnCooldown(ReducerContext& ctx, uint64_t MonsterId, const std::string& PlayerName)
{
    const std::string Key = MonsterHitCooldownKey(MonsterId, PlayerName);
    if (auto Row = ctx.db[monster_attack_hit_cooldown_cooldown_key].find(Key))
    {
        const TimeDuration Since = ctx.timestamp.duration_since(Row->last_hit_at);
        if (Since.millis() < kMonsterAttackHitCooldownMillis)
        {
            return true;
        }
    }

    return false;
}

void RecordMonsterAttackHit(ReducerContext& ctx, uint64_t MonsterId, const std::string& PlayerName)
{
    const std::string Key = MonsterHitCooldownKey(MonsterId, PlayerName);
    const MonsterAttackHitCooldown Row{Key, ctx.timestamp};
    if (auto Existing = ctx.db[monster_attack_hit_cooldown_cooldown_key].find(Key))
    {
        *Existing = Row;
        ctx.db[monster_attack_hit_cooldown_cooldown_key].update(*Existing);
    }
    else
    {
        ctx.db[monster_attack_hit_cooldown].insert(Row);
    }
}

void ApplyDamageToPlayerRow(ReducerContext& ctx, Player& PlayerRow, float Amount)
{
    if (PlayerRow.health <= 0.f)
    {
        return;
    }

    const float NextHealth = PlayerRow.health - Amount;
    PlayerRow.health = NextHealth > 0.f ? NextHealth : 0.f;
    if (PlayerRow.health <= 0.f)
    {
        PlayerRow.is_attacking = false;
    }
    PlayerRow.updated_at = ctx.timestamp;
    ctx.db[player_name].update(PlayerRow);
    if (PlayerRow.health <= 0.f)
    {
        SchedulePlayerRespawn(ctx, PlayerRow);
    }
}

float DeltaDegrees(float From, float To)
{
    float Delta = std::fmod(To - From, 360.f);
    if (Delta > 180.f)
    {
        Delta -= 360.f;
    }
    else if (Delta < -180.f)
    {
        Delta += 360.f;
    }
    return Delta;
}

float TurnToward(float Yaw, float DesiredYaw, float TurnRate, float DeltaSeconds)
{
    const float Delta = DeltaDegrees(Yaw, DesiredYaw);
    const float MaxStep = TurnRate * DeltaSeconds;
    if (std::fabs(Delta) <= MaxStep)
    {
        return DesiredYaw;
    }

    return Yaw + std::copysign(MaxStep, Delta);
}

float HorizontalDistanceSquared(const DbVector3& From, const DbVector3& To)
{
    const float Dx = To.x - From.x;
    const float Dy = To.y - From.y;
    return Dx * Dx + Dy * Dy;
}

bool FindClosestOnlinePlayer(
    ReducerContext& ctx,
    const DbVector3& Origin,
    float Range,
    DbVector3& OutPosition,
    float& OutDistance)
{
    const float RangeSquared = Range * Range;
    bool bFound = false;
    float BestDistanceSquared = 0.f;

    for (const auto& Row : ctx.db[player])
    {
        if (!Row.online)
        {
            continue;
        }

        const float DistanceSquared = HorizontalDistanceSquared(Origin, Row.position);
        if (DistanceSquared > RangeSquared)
        {
            continue;
        }

        if (!bFound || DistanceSquared < BestDistanceSquared)
        {
            bFound = true;
            BestDistanceSquared = DistanceSquared;
            OutPosition = Row.position;
            OutDistance = std::sqrt(DistanceSquared);
        }
    }

    return bFound;
}

bool MonsterStateChanged(const Monster& Before, const Monster& After)
{
    if (Before.is_attacking != After.is_attacking || Before.loop_attack != After.loop_attack)
    {
        return true;
    }

    if (std::fabs(Before.position.x - After.position.x) > kMonsterStateEpsilon
        || std::fabs(Before.position.y - After.position.y) > kMonsterStateEpsilon
        || std::fabs(Before.position.z - After.position.z) > kMonsterStateEpsilon)
    {
        return true;
    }

    return std::fabs(DeltaDegrees(Before.rotation.yaw, After.rotation.yaw)) > kMonsterStateEpsilon
        || std::fabs(Before.rotation.pitch - After.rotation.pitch) > kMonsterStateEpsilon
        || std::fabs(Before.rotation.roll - After.rotation.roll) > kMonsterStateEpsilon;
}

void TickOneMonster(ReducerContext& ctx, Monster Row)
{
    if (Row.health <= 0.f)
    {
        return;
    }

    const Monster Before = Row;
    const MonsterKindConfig Config = ConfigForKind(Row.kind);
    const float DetectionRange = EffectiveDetectionRange(Row);
    const float AttackRange = EffectiveAttackRange(Row);
    DbVector3 Target{};
    float Distance = 0.f;
    const bool bHasTarget = FindClosestOnlinePlayer(ctx, Row.position, DetectionRange, Target, Distance);

    Row.position.z = Row.spawn_position.z;
    Row.rotation.pitch = 0.f;
    Row.rotation.roll = 0.f;

    if (!bHasTarget)
    {
        Row.is_attacking = false;
        Row.loop_attack = false;
    }
    else
    {
        if (Distance > kMonsterStateEpsilon)
        {
            const float DesiredYaw = std::atan2(Target.y - Row.position.y, Target.x - Row.position.x) * kDegreesPerRadian;
            Row.rotation.yaw = TurnToward(Row.rotation.yaw, DesiredYaw, Config.TurnRate, kMonsterTickSeconds);

            if (Config.Speed > 0.f && Distance > Config.StopDistance)
            {
                const float Step = std::min(Config.Speed * kMonsterTickSeconds, Distance - Config.StopDistance);
                const float Scale = Step / Distance;
                Row.position.x += (Target.x - Row.position.x) * Scale;
                Row.position.y += (Target.y - Row.position.y) * Scale;
            }
        }

        const bool bAttack = Distance <= AttackRange;
        Row.is_attacking = bAttack;
        Row.loop_attack = bAttack;
    }

    if (!MonsterStateChanged(Before, Row))
    {
        return;
    }

    Row.updated_at = ctx.timestamp;
    ctx.db[monster_id].update(Row);
}

void TickMonsters(ReducerContext& ctx)
{
    bool bAnyOnline = false;
    for (const auto& Row : ctx.db[player])
    {
        if (Row.online)
        {
            bAnyOnline = true;
            break;
        }
    }

    std::vector<Monster> Rows;
    for (const auto& Row : ctx.db[monster])
    {
        Rows.push_back(Row);
    }

    if (!bAnyOnline)
    {
        for (Monster& Row : Rows)
        {
            if (!Row.is_attacking && !Row.loop_attack)
            {
                continue;
            }

            Row.is_attacking = false;
            Row.loop_attack = false;
            Row.updated_at = ctx.timestamp;
            ctx.db[monster_id].update(Row);
        }
        return;
    }

    for (const Monster& Row : Rows)
    {
        TickOneMonster(ctx, Row);
    }
}

bool HasPendingRespawn(ReducerContext& ctx, const std::string& Name)
{
    for (const auto& Row : ctx.db[player_respawn_timer])
    {
        if (Row.name == Name)
        {
            return true;
        }
    }

    return false;
}

// (0, 0, 0) with a zero rotator is the unset transform. Clients replace it with the level spawn anchor.
void SchedulePlayerRespawn(ReducerContext& ctx, const Player& PlayerRow)
{
    if (PlayerRow.health > 0.f || HasPendingRespawn(ctx, PlayerRow.name))
    {
        return;
    }

    ctx.db[player_respawn_timer].insert(PlayerRespawnTimer{
        0,
        ScheduleAt::time(ctx.timestamp + TimeDuration::from_millis(kPlayerRespawnDelayMillis)),
        PlayerRow.name
    });
}

bool HasPendingMonsterRespawn(ReducerContext& ctx, uint64_t MonsterId)
{
    for (const auto& Row : ctx.db[monster_respawn_timer])
    {
        if (Row.monster_id == MonsterId)
        {
            return true;
        }
    }

    return false;
}

void ScheduleMonsterRespawn(ReducerContext& ctx, const Monster& MonsterRow)
{
    if (MonsterRow.health > 0.f || HasPendingMonsterRespawn(ctx, MonsterRow.id))
    {
        return;
    }

    ctx.db[monster_respawn_timer].insert(MonsterRespawnTimer{
        0,
        ScheduleAt::time(ctx.timestamp + TimeDuration::from_millis(kMonsterRespawnDelayMillis)),
        MonsterRow.id
    });
}

void EnsureMonsterTick(ReducerContext& ctx)
{
    if (ctx.db[monster_tick_timer].count() > 0)
    {
        return;
    }

    ctx.db[monster_tick_timer].insert(MonsterTickTimer{
        0,
        ScheduleAt::interval(TimeDuration::from_millis(kMonsterTickMillis))
    });
    LOG_INFO("Scheduled monster_tick at 20 Hz");
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
    EnsureMonsterTick(ctx);
    return Ok();
}

// init does not re-run on an existing database. Call these once after publish.
SPACETIMEDB_REDUCER(seed_monsters, ReducerContext ctx)
{
    SeedMonstersIfEmpty(ctx);
    return Ok();
}

SPACETIMEDB_REDUCER(ensure_monster_tick, ReducerContext ctx)
{
    EnsureMonsterTick(ctx);
    return Ok();
}

SPACETIMEDB_REDUCER(monster_tick, ReducerContext ctx, MonsterTickTimer timer)
{
    (void)timer;
    TickMonsters(ctx);
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
        SchedulePlayerRespawn(ctx, *player);
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
        role,
        kPlayerMaxHealth,
        false
    });
    return Ok();
}

// Subtracts health from the logged-in caller. The row is the only place health changes.
SPACETIMEDB_REDUCER(damage_player, ReducerContext ctx, float amount)
{
    if (!std::isfinite(amount) || amount <= 0.f)
    {
        return Err("Damage must be greater than zero");
    }

    if (!ctx.connection_id)
    {
        return Err("Not logged in");
    }

    const auto Session = ctx.db[player_session_connection_id].find(*ctx.connection_id);
    if (!Session)
    {
        return Err("Not logged in");
    }

    auto PlayerRow = ctx.db[player_name].find(Session->name);
    if (!PlayerRow || !PlayerRow->online)
    {
        return Err("Not logged in");
    }

    if (PlayerRow->health <= 0.f)
    {
        return Ok();
    }

    const float NextHealth = PlayerRow->health - amount;
    PlayerRow->health = NextHealth > 0.f ? NextHealth : 0.f;
    if (PlayerRow->health <= 0.f)
    {
        PlayerRow->is_attacking = false;
    }
    PlayerRow->updated_at = ctx.timestamp;
    ctx.db[player_name].update(*PlayerRow);
    if (PlayerRow->health <= 0.f)
    {
        SchedulePlayerRespawn(ctx, *PlayerRow);
    }
    return Ok();
}

// Local client calls after a pawn overlap at the attack hit frame. The server checks range and cooldown.
SPACETIMEDB_REDUCER(monster_attack_player, ReducerContext ctx, uint64_t target_monster_id)
{
    if (!ctx.connection_id)
    {
        return Err("Not logged in");
    }

    const auto Session = ctx.db[player_session_connection_id].find(*ctx.connection_id);
    if (!Session)
    {
        return Err("Not logged in");
    }

    if (target_monster_id == 0)
    {
        return Err("Invalid monster");
    }

    auto MonsterRow = ctx.db[monster_id].find(target_monster_id);
    if (!MonsterRow || MonsterRow->health <= 0.f)
    {
        return Ok();
    }

    if (!MonsterRow->is_attacking)
    {
        return Err("Monster is not attacking");
    }

    auto PlayerRow = ctx.db[player_name].find(Session->name);
    if (!PlayerRow || !PlayerRow->online)
    {
        return Err("Not logged in");
    }

    if (PlayerRow->health <= 0.f)
    {
        return Ok();
    }

    const float Range = EffectiveAttackRange(*MonsterRow) + kMonsterHitRangeSlack;
    const float DistanceSquared = HorizontalDistanceSquared(MonsterRow->position, PlayerRow->position);
    if (DistanceSquared > Range * Range)
    {
        return Err("Out of range");
    }

    if (IsMonsterAttackHitOnCooldown(ctx, target_monster_id, Session->name))
    {
        return Ok();
    }

    RecordMonsterAttackHit(ctx, target_monster_id, Session->name);
    ApplyDamageToPlayerRow(ctx, *PlayerRow, EffectiveAttackDamage(*MonsterRow));
    return Ok();
}

// Subtracts health from a monster row. Any logged-in client may call this for testing.
SPACETIMEDB_REDUCER(damage_monster, ReducerContext ctx, uint64_t target_id, float amount)
{
    if (!std::isfinite(amount) || amount <= 0.f)
    {
        return Err("Damage must be greater than zero");
    }

    if (!ctx.connection_id)
    {
        return Err("Not logged in");
    }

    const auto Session = ctx.db[player_session_connection_id].find(*ctx.connection_id);
    if (!Session)
    {
        return Err("Not logged in");
    }

    if (target_id == 0)
    {
        return Err("Invalid monster");
    }

    auto MonsterRow = ctx.db[monster_id].find(target_id);
    if (!MonsterRow)
    {
        return Err("Unknown monster");
    }

    if (MonsterRow->health <= 0.f)
    {
        return Ok();
    }

    const float NextHealth = MonsterRow->health - amount;
    MonsterRow->health = NextHealth > 0.f ? NextHealth : 0.f;
    MonsterRow->updated_at = ctx.timestamp;
    if (MonsterRow->health <= 0.f)
    {
        MonsterRow->is_attacking = false;
        MonsterRow->loop_attack = false;
    }

    ctx.db[monster_id].update(*MonsterRow);
    if (MonsterRow->health <= 0.f)
    {
        ScheduleMonsterRespawn(ctx, *MonsterRow);
    }

    return Ok();
}

// Runs one second after health reaches 0. Full health at the authored spawn transform.
SPACETIMEDB_REDUCER(respawn_monster, ReducerContext ctx, MonsterRespawnTimer timer)
{
    if (ctx.connection_id)
    {
        return Err("Not a client reducer");
    }

    auto MonsterRow = ctx.db[monster_id].find(timer.monster_id);
    if (!MonsterRow || MonsterRow->health > 0.f)
    {
        return Ok();
    }

    MonsterRow->health = kMonsterMaxHealth;
    MonsterRow->position = MonsterRow->spawn_position;
    MonsterRow->rotation = MonsterRow->spawn_rotation;
    MonsterRow->is_attacking = false;
    MonsterRow->loop_attack = false;
    MonsterRow->updated_at = ctx.timestamp;
    ctx.db[monster_id].update(*MonsterRow);
    return Ok();
}

// Runs one second after health reaches 0. Full health, and the unset transform so clients use the level spawn.
SPACETIMEDB_REDUCER(respawn_player, ReducerContext ctx, PlayerRespawnTimer timer)
{
    if (ctx.connection_id)
    {
        return Err("Not a client reducer");
    }

    auto PlayerRow = ctx.db[player_name].find(timer.name);
    if (!PlayerRow || PlayerRow->health > 0.f)
    {
        return Ok();
    }

    PlayerRow->health = kPlayerMaxHealth;
    PlayerRow->position = DbVector3{0.f, 0.f, 0.f};
    PlayerRow->rotation = DbRotator{0.f, 0.f, 0.f};
    PlayerRow->is_attacking = false;
    PlayerRow->updated_at = ctx.timestamp;
    ctx.db[player_name].update(*PlayerRow);
    return Ok();
}

// The caller is resolved only through the session for ctx.connection_id.
SPACETIMEDB_REDUCER(update_player_transform, ReducerContext ctx, DbVector3 position, DbRotator rotation, bool is_attacking)
{
    if (!UpdateOnlinePlayerTransform(ctx, position, rotation, is_attacking))
    {
        return Err("Not logged in");
    }

    return Ok();
}
