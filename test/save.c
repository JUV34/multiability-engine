#include "global.h"
#include "malloc.h"
#include "pokemon_storage_system.h"
#include "recorded_battle.h"
#include "save.h"
#include "test/test.h"

// If you would like to ensure save compatibility, update the values below with those for your hack. You can find these through the debug menu.
// Please note that this simple check is not 100% foolproof, but should be able to catch most unintended shifts.
#define T_SAVEBLOCK1_SIZE 15568
#define T_SAVEBLOCK2_SIZE 3884
#define T_SAVEBLOCK3_SIZE 4
#define T_POKEMONSTORAGE_SIZE 34144

TEST("SaveBlock1 is backwards compatible")
{
    EXPECT_EQ(sizeof(struct SaveBlock1), T_SAVEBLOCK1_SIZE);
}

TEST("SaveBlock2 is backwards compatible")
{
    EXPECT_EQ(sizeof(struct SaveBlock2), T_SAVEBLOCK2_SIZE);
}

TEST("SaveBlock3 is backwards compatible")
{
    EXPECT_EQ(sizeof(struct SaveBlock3), T_SAVEBLOCK3_SIZE);
}

TEST("PokemonStorage is backwards compatible")
{
    EXPECT_EQ(sizeof(struct PokemonStorage), T_POKEMONSTORAGE_SIZE);
}

TEST("MS1-R PC tail records names wallpapers and fusions fit serialized chunks")
{
    u32 capacity = SECTOR_DATA_SIZE * (SECTOR_ID_PKMN_STORAGE_END - SECTOR_ID_PKMN_STORAGE_START + 1);
    struct PokemonStorage *storage = AllocZeroed(sizeof(*storage));
    u8 *serialized = AllocZeroed(capacity);
    u8 *raw = (u8 *)storage;
    EXPECT(storage != NULL);
    EXPECT(serialized != NULL);
    EXPECT_EQ(sizeof(struct BoxPokemon), 80);
    EXPECT_EQ(sizeof(struct Pokemon), 100);
    EXPECT_EQ(sizeof(*storage), 34144);
    EXPECT_EQ(capacity - sizeof(*storage), 1568);
    EXPECT_LE(offsetof(struct PokemonStorage, boxes) + sizeof(storage->boxes), capacity);
    EXPECT_LE(offsetof(struct PokemonStorage, boxNames) + sizeof(storage->boxNames), capacity);
    EXPECT_LE(offsetof(struct PokemonStorage, boxWallpapers) + sizeof(storage->boxWallpapers), capacity);
    EXPECT_LE(offsetof(struct PokemonStorage, fusions) + sizeof(storage->fusions), capacity);

    // In-memory chunk round-trip, not a flash save/reload fixture.
    for (u32 i = 0; i < sizeof(*storage); i++)
        raw[i] = (i * 31 + 7) & 0xFF;
    for (u32 offset = 0; offset < sizeof(*storage); offset += SECTOR_DATA_SIZE)
        memcpy(serialized + offset, raw + offset, min(SECTOR_DATA_SIZE, sizeof(*storage) - offset));
    memset(storage, 0, sizeof(*storage));
    for (u32 offset = 0; offset < sizeof(*storage); offset += SECTOR_DATA_SIZE)
        memcpy(raw + offset, serialized + offset, min(SECTOR_DATA_SIZE, sizeof(*storage) - offset));
    for (u32 i = 0; i < sizeof(*storage); i++)
        EXPECT_EQ(raw[i], (i * 31 + 7) & 0xFF);
    for (u32 i = sizeof(*storage); i < capacity; i++)
        EXPECT_EQ(serialized[i], 0);
    Free(serialized);
    Free(storage);
}

TEST("MS1-R recorded battle copies fit payload and allocation bounds")
{
    struct RecordedBattleSave *record = AllocZeroed(sizeof(*record));
    u8 *sector = AllocZeroed(SECTOR_SIZE + 16);
    EXPECT(record != NULL);
    EXPECT(sector != NULL);
    EXPECT_EQ(sizeof(*record), 4092);
    EXPECT_EQ(sizeof(*record), SECTOR_COUNTER_OFFSET);
    EXPECT_EQ(offsetof(struct RecordedBattleSave, checksum), 4088);
    EXPECT_LE(sizeof(*record), SECTOR_SIZE);
    memset(record, 0x5A, sizeof(*record));
    memset(sector + SECTOR_SIZE, 0xA5, 16);
    memcpy(sector, record, sizeof(*record));
    for (u32 i = sizeof(*record); i < SECTOR_SIZE; i++)
        EXPECT_EQ(sector[i], 0);
    for (u32 i = SECTOR_SIZE; i < SECTOR_SIZE + 16; i++)
        EXPECT_EQ(sector[i], 0xA5);
    memset(record, 0, sizeof(*record));
    memcpy(record, sector, sizeof(*record));
    for (u32 i = 0; i < sizeof(*record); i++)
        EXPECT_EQ(((u8 *)record)[i], 0x5A);
    Free(sector);
    Free(record);
}

#undef T_SAVEBLOCK1_SIZE
#undef T_SAVEBLOCK2_SIZE
#undef T_SAVEBLOCK3_SIZE
#undef T_POKEMONSTORAGE_SIZE
