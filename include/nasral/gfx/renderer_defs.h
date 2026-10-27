#pragma once

#define FOREACH_FRAME for (size_t FRAME = 0; FRAME < config().max_frames_in_flight; ++FRAME) {

#define FOREACH_IMAGE for (size_t IMAGE = 0; IMAGE < config().swap_chain_images; ++IMAGE) {

#define END_FOREACH }

#define ENSURE_COMMANDS if(!is_active() || !is_frame_started()) return;

#define ENSURE_PASS if(!is_active() || !is_pass_started()) return;

#define ENSURE_NO_PASS if(!is_active() || is_pass_started()) return;