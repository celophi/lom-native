#ifndef _CARD_CALLBACKS_H
#define _CARD_CALLBACKS_H

#include "common.h"
#include "common/saved_game.h"

#define CARD_RESOURCE_HEADER_SIZE 768
#define CARD_REWARD_COUNT 50
#define CARD_FIRST_SPIRIT_LAND 7
#define CARD_LAST_LAND 31
#define CARD_SPIRIT_LAND_COUNT 20

typedef struct
{
    s32 growth_delta;
    u32 reward_count;
    u8 rewards[CARD_REWARD_COUNT];
    u8 reserved[2];
    u8 egg_species;
    u8 level;
    u8 early_land_count;
    u8 land_count;
    s32 land_bits;
    u8 maxed_spirits[CARD_SPIRIT_LAND_COUNT];
    PetRecord pet;
} CardPetTransfer;

typedef struct
{
    u8 header[CARD_RESOURCE_HEADER_SIZE];
    CardPetTransfer transfer;
} CardPetResource;

void card_prepare_pet_transfer(u8* resource, struct PetRecord* pet);

#endif
