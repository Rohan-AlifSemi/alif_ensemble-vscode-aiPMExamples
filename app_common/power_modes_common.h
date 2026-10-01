#include <aipm.h>
#include <stdint.h>

void execute_go_mode_usecase(uint32_t mode_number);
void execute_ready_mode_usecase(uint32_t mode_number);
void execute_idle_mode_usecase(uint32_t mode_number);
void execute_standby_mode_usecase(uint32_t mode_number);
void execute_stop_mode_usecase(uint32_t mode_number);

void configure_go_mode_profiles(uint32_t mode_number,
        run_profile_t *runp, off_profile_t *offp);
void configure_ready_mode_profiles(uint32_t mode_number,
        run_profile_t *runp, off_profile_t *offp);
void configure_idle_mode_profiles(uint32_t mode_number,
        run_profile_t *runp, off_profile_t *offp);
void configure_standby_mode_profiles(uint32_t mode_number,
        run_profile_t *runp, off_profile_t *offp);
void configure_stop_mode_profiles(uint32_t mode_number,
        run_profile_t *runp, off_profile_t *offp);