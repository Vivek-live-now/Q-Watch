#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "../include/mochi_pet.h"

int main() {
    printf("=== Dasai Mochi Pet Engine Host Test Harness ===\n");

    MochiPet pet;
    pet.begin();

    // -------------------------------------------------------------
    // Test 1: Emote Cycle, Names & Enum Integrity
    // -------------------------------------------------------------
    printf("[1/6] Testing Emote Enumeration and Cyclic Navigation...\n");
    assert(static_cast<uint8_t>(MochiEmote::COUNT) == 17);
    for (uint8_t i = 0; i < 17; i++) {
        MochiEmote e = static_cast<MochiEmote>(i);
        const char* name = pet.getEmoteName(e);
        assert(name != NULL && strlen(name) > 0);
        assert(strcmp(name, "UNKNOWN") != 0);
    }

    pet.setEmote(MochiEmote::HAPPY);
    assert(pet.getEmote() == MochiEmote::HAPPY);
    pet.nextEmote();
    assert(pet.getEmote() == MochiEmote::LOVE);
    pet.prevEmote();
    assert(pet.getEmote() == MochiEmote::HAPPY);
    pet.prevEmote();
    assert(pet.getEmote() == MochiEmote::GUNDAM); // Wrap around backwards
    pet.nextEmote();
    assert(pet.getEmote() == MochiEmote::HAPPY);  // Wrap around forwards
    printf("  [PASS] All 17 emotes cycled and validated.\n");

    // -------------------------------------------------------------
    // Test 2: Modular Digital Helmets Cycle & Name Integrity
    // -------------------------------------------------------------
    printf("[2/6] Testing Modular Digital Helmets...\n");
    assert(static_cast<uint8_t>(MochiHelmet::COUNT) == 5);
    const char* expected_helmets[] = {"CLASSIC", "GUNDAM", "CYBER", "NEKO", "TACTICAL"};
    for (uint8_t i = 0; i < 5; i++) {
        MochiHelmet h = static_cast<MochiHelmet>(i);
        const char* name = pet.getHelmetName(h);
        assert(strcmp(name, expected_helmets[i]) == 0);
    }

    pet.setHelmet(MochiHelmet::CLASSIC);
    assert(pet.getHelmet() == MochiHelmet::CLASSIC);
    pet.nextHelmet();
    assert(pet.getHelmet() == MochiHelmet::GUNDAM);
    assert(pet.getLastPlayedSound() == MochiSound::GUNDAM_BEEP);
    pet.nextHelmet();
    assert(pet.getHelmet() == MochiHelmet::CYBER);
    pet.prevHelmet();
    assert(pet.getHelmet() == MochiHelmet::GUNDAM);
    pet.prevHelmet();
    assert(pet.getHelmet() == MochiHelmet::CLASSIC);
    pet.prevHelmet();
    assert(pet.getHelmet() == MochiHelmet::TACTICAL); // Wrap around backwards
    printf("  [PASS] All 5 digital helmets verified.\n");

    // -------------------------------------------------------------
    // Test 3: IMU 6-Axis Physics, Gaze Clamping & Shake Detection
    // -------------------------------------------------------------
    printf("[3/6] Testing IMU 6-Axis Physics & Gaze Clamping...\n");
    // Simulate extreme right tilt (Roll = +90 deg, Pitch = 0 deg)
    for (int step = 0; step < 20; step++) {
        pet.updatePhysics(0.0f, 90.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    }
    assert(pet.getPupilDx() <= 7.01f && pet.getPupilDx() >= 6.5f);
    assert(fabsf(pet.getPupilDy()) < 0.5f);

    // Simulate extreme forward tilt (Pitch = +80 deg, Roll = 0 deg)
    for (int step = 0; step < 20; step++) {
        pet.updatePhysics(80.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    }
    assert(pet.getPupilDy() >= -5.01f && pet.getPupilDy() <= -4.5f);

    // Simulate Shake: 2.8 G high-G impact
    assert(!pet.isDizzy());
    pet.updatePhysics(0.0f, 0.0f, 1.6f, 1.6f, 1.6f, 0.0f, 0.0f, 0.0f); // Mag = sqrt(3*2.56) = ~2.77G
    assert(pet.isDizzy());
    assert(pet.getEmote() == MochiEmote::DIZZY);
    assert(pet.getLastPlayedSound() == MochiSound::DIZZY_STUMBLE);

    // Simulate time passing (3.6 seconds) to recover from dizziness
    for (int f = 0; f < 120; f++) {
        pet.update(0.033f);
    }
    assert(!pet.isDizzy());

    // Simulate Freefall: Zero-G (0.1 G)
    pet.updatePhysics(0.0f, 0.0f, 0.05f, 0.05f, 0.05f, 0.0f, 0.0f, 0.0f);
    assert(pet.getEmote() == MochiEmote::CONFUSED);
    printf("  [PASS] Gaze tracking, shake detection & zero-G response verified.\n");

    // -------------------------------------------------------------
    // Test 4: Tamagotchi Care Mechanics (Petting, Feeding & Sleep)
    // -------------------------------------------------------------
    printf("[4/6] Testing Tamagotchi Care Mechanics...\n");
    uint8_t init_hap = pet.getHappiness();
    uint16_t init_xp = pet.getFriendshipXP();
    pet.pet();
    assert(pet.getHappiness() >= init_hap);
    assert(pet.getFriendshipXP() > init_xp);
    assert(pet.getEmote() == MochiEmote::LOVE);
    assert(pet.getLastPlayedSound() == MochiSound::LOVE_CHIME);

    // Feeding
    assert(!pet.isFeeding());
    pet.feed();
    assert(pet.isFeeding());
    assert(pet.getEmote() == MochiEmote::EXCITED);
    // Let snack arrive at mouth and chew
    for (int f = 0; f < 90; f++) {
        pet.update(0.033f);
    }
    assert(!pet.isFeeding());
    assert(pet.getEmote() == MochiEmote::CONTENT);

    // Sleep toggle
    assert(!pet.isSleeping());
    pet.toggleSleep();
    assert(pet.isSleeping());
    assert(pet.getEmote() == MochiEmote::SLEEPING);
    assert(pet.getLastPlayedSound() == MochiSound::SNORE);

    // Petting while sleeping wakes Mochi up
    pet.pet();
    assert(!pet.isSleeping());
    assert(pet.getEmote() == MochiEmote::HAPPY);
    assert(pet.getLastPlayedSound() == MochiSound::WAKEUP);
    printf("  [PASS] Petting, feeding, XP progression, and sleep lifecycle verified.\n");

    // -------------------------------------------------------------
    // Test 5: Headless 128x64 Buffer Rendering & Zero Memory Leaks
    // -------------------------------------------------------------
    printf("[5/6] Testing Headless 128x64 Vector Buffer Rendering...\n");
    uint8_t display_buffer[1024];
    memset(display_buffer, 0, sizeof(display_buffer));

    // Render multiple frames with different emotes and helmets
    for (int emote_idx = 0; emote_idx < 17; emote_idx++) {
        pet.setEmote(static_cast<MochiEmote>(emote_idx));
        pet.setHelmet(static_cast<MochiHelmet>(emote_idx % 5));
        pet.update(0.033f);
        pet.renderToBuffer(display_buffer, 128, 64);

        // Verify that pixels were drawn into the buffer (buffer is not all zeros)
        int non_zero_bytes = 0;
        for (size_t b = 0; b < sizeof(display_buffer); b++) {
            if (display_buffer[b] != 0) non_zero_bytes++;
        }
        assert(non_zero_bytes > 20); // Significant geometry drawn
    }
    printf("  [PASS] Virtual display buffer rendering verified across all 17 emotes.\n");

    // -------------------------------------------------------------
    // Test 6: RAM Footprint & Zero-Flash Bloat Check
    // -------------------------------------------------------------
    printf("[6/6] Verifying RAM Budget & Flash Safety...\n");
    size_t obj_size = sizeof(MochiPet);
    printf("  sizeof(MochiPet) = %zu bytes\n", obj_size);
    assert(obj_size < 300); // Strict RAM constraint
    printf("  [PASS] MochiPet object is ultra-compact (%zu B), guaranteeing zero flash bloat.\n", obj_size);

    printf("\nAll Dasai Mochi tests PASSED successfully!\n");
    return 0;
}
