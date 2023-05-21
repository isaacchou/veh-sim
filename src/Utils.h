/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#pragma once

#include <chrono>

void debug_log(const char* format, ...);
void debug_log_mute(const char* format, ...);

class Timer
{
private:
	std::chrono::high_resolution_clock m_timer;
	std::chrono::high_resolution_clock::time_point m_start_time;

public:
	Timer() { m_start_time = m_timer.now(); }
	float get_elapsed_time() {
		std::chrono::high_resolution_clock::time_point cur_time = m_timer.now();
		using fseconds = std::chrono::duration<float>;
		float elapsed_time = std::chrono::duration_cast<fseconds>(cur_time - m_start_time).count();
		return elapsed_time;
	}
};
