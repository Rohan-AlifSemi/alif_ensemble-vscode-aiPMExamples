#include <alif.h>
#include <se_services_port.h>
#include <services_lib_api.h>

#if defined(M55_HP)
#define CLOCK_FREQUENCY_CPU         CLOCK_FREQUENCY_400MHZ
#elif defined(M55_HE)
#define CLOCK_FREQUENCY_CPU         CLOCK_FREQUENCY_160MHZ
#endif

void execute_go_mode_usecase(uint32_t mode_number)
{
    uint32_t ret, service_resp;
    volatile uint32_t debug_loop = 1;

    switch(mode_number+1) {
    case 1:
        ret = SERVICES_clocks_set_divider(se_services_s_handle, DIVIDER_ACLK, 1, &service_resp);

        /* function call to exercise CPU & NPU here */
        while(1) if (debug_loop == 0) break;

        break;
    case 2:
        ret = SERVICES_clocks_set_divider(se_services_s_handle, DIVIDER_ACLK, 1, &service_resp);

        /* function call to exercise CPU here */
        while(1) if (debug_loop == 0) break;

        break;
    case 3:
        ret = SERVICES_clocks_set_divider(se_services_s_handle, DIVIDER_ACLK, 1, &service_resp);

#if defined(M55_HE) && !defined(ENSEMBLE_SOC_E1C)
        /* go to subsystem off */
        while(1) 
        {
          __disable_irq();
          pm_core_enter_deep_sleep_request_subsys_off();
          __enable_irq();
        }
#endif
        /* function call to exercise CPU here */
        while(1) if (debug_loop == 0) break;

        break;
    case 4:
    case 5:
#if defined(M55_HP)
        /* go to subsystem off */
        while(1) 
        {
          __disable_irq();
          pm_core_enter_deep_sleep_request_subsys_off();
          __enable_irq();
        }
#endif
        /* function call to exercise CPU here */
        while(1) if (debug_loop == 0) break;

        break;
    default:
        /* this should not happen */
        break;
    }
}

void configure_go_mode_profiles(uint32_t mode_number,
        run_profile_t *runp, off_profile_t *offp)
{
    /* change to CLK_SRC_LFRC if 32k XTAL not installed */
    runp->aon_clk_src = CLK_SRC_LFXO;
    offp->aon_clk_src = CLK_SRC_LFXO;

    runp->memory_blocks = MRAM_MASK | BACKUP4K_MASK;
    offp->memory_blocks = MRAM_MASK | BACKUP4K_MASK;

    runp->vdd_ioflex_3V3 = IOFLEX_LEVEL_1V8;
    offp->vdd_ioflex_3V3 = IOFLEX_LEVEL_1V8;

    switch(mode_number+1) {
    case 1:
    case 2:
        runp->run_clk_src = CLK_SRC_PLL;
        runp->cpu_clk_freq = CLOCK_FREQUENCY_CPU;
        runp->scaled_clk_freq = SCALED_FREQ_RC_ACTIVE_76_8_MHZ;

        runp->dcdc_mode = DCDC_MODE_PWM;
        runp->dcdc_voltage = 825;

        #if SOC_FEAT_HAS_BULK_SRAM
        runp->memory_blocks |= SRAM0_MASK | SRAM1_MASK;
        #endif
        runp->power_domains = PD_SYST_MASK;
        offp->power_domains = PD_SYST_MASK;

        break;
    case 3:
        runp->run_clk_src = CLK_SRC_PLL;
        runp->cpu_clk_freq = CLOCK_FREQUENCY_CPU;
        runp->scaled_clk_freq = SCALED_FREQ_RC_ACTIVE_76_8_MHZ;

        runp->dcdc_mode = DCDC_MODE_PWM;
        runp->dcdc_voltage = 825;

#if !defined(ENSEMBLE_SOC_E1C)
        runp->power_domains = PD_SYST_MASK;
        offp->power_domains = PD_SYST_MASK;
#endif

        break;
    case 4:
        /* the actual cpu_clk_freq is 76.8 MHz, it follows the scaled_clk_freq */
        runp->run_clk_src = CLK_SRC_HFRC;
        runp->cpu_clk_freq = CLOCK_FREQUENCY_76_8_RC_MHZ;
        runp->scaled_clk_freq = SCALED_FREQ_RC_ACTIVE_76_8_MHZ;

        runp->dcdc_mode = DCDC_MODE_PFM_FORCED;
        runp->dcdc_voltage = 775;

        runp->power_domains = PD_SSE700_AON_MASK;    /* PD6 SYST is OFF */
        offp->power_domains = PD_SSE700_AON_MASK;

        break;
    case 5:
        /* the actual cpu_clk_freq is 19.2 MHz, it follows the scaled_clk_freq */
        runp->run_clk_src = CLK_SRC_HFRC;
        runp->cpu_clk_freq = CLOCK_FREQUENCY_76_8_RC_MHZ;
        runp->scaled_clk_freq = SCALED_FREQ_RC_ACTIVE_19_2_MHZ;

        runp->dcdc_mode = DCDC_MODE_PFM_FORCED;
        runp->dcdc_voltage = 775;

        runp->power_domains = PD_SSE700_AON_MASK;    /* PD6 SYST is OFF */
        offp->power_domains = PD_SSE700_AON_MASK;

        break;
    default:
        /* this should not happen */
        break;
    }
}
