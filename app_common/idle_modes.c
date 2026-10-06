#include <alif.h>
#include <pm_soc_clk.h>
#include <se_services_port.h>
#include <services_lib_api.h>

#if defined(M55_HP)
#define CLOCK_FREQUENCY_CPU         CLOCK_FREQUENCY_400MHZ
#elif defined(M55_HE)
#define CLOCK_FREQUENCY_CPU         CLOCK_FREQUENCY_160MHZ
#endif

void execute_idle_mode_usecase(uint32_t mode_number)
{
    /* go to subsystem off */
    while(1) 
    {
        __disable_irq(); 
        pm_core_enter_deep_sleep_request_subsys_off();
        __enable_irq();
    }
}

void configure_idle_mode_profiles(uint32_t mode_number,
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
        runp->run_clk_src = CLK_SRC_HFXO;
        runp->cpu_clk_freq = CLOCK_FREQUENCY_76_8_XO_MHZ;
        runp->scaled_clk_freq = SCALED_FREQ_RC_ACTIVE_38_4_MHZ;

#if defined(M55_HP)
        runp->dcdc_mode = DCDC_MODE_PWM;
#else
        runp->dcdc_mode = DCDC_MODE_PFM_FORCED;
#endif
        runp->dcdc_voltage = 775;

        runp->power_domains = PD_SYST_MASK;
        offp->power_domains = PD_SYST_MASK;

        offp->memory_blocks |= SRAM4_1_MASK | SRAM4_2_MASK | SRAM5_1_MASK | SRAM5_2_MASK;
        offp->vtor_address = SCB->VTOR;

        break;
    case 2:
        /* the actual cpu_clk_freq is 0.6 MHz, it follows the scaled_clk_freq */
        runp->run_clk_src = CLK_SRC_HFRC;
        runp->cpu_clk_freq = CLOCK_FREQUENCY_76_8_RC_MHZ;
        runp->scaled_clk_freq = SCALED_FREQ_RC_ACTIVE_0_6_MHZ;

#if defined(M55_HP)
        runp->dcdc_mode = DCDC_MODE_PWM;
#else
        runp->dcdc_mode = DCDC_MODE_PFM_FORCED;
#endif
        runp->dcdc_voltage = 775;

        runp->power_domains = PD_SYST_MASK;
        offp->power_domains = PD_SYST_MASK;

        offp->memory_blocks |= SRAM4_1_MASK | SRAM4_2_MASK | SRAM5_1_MASK | SRAM5_2_MASK;
        offp->vtor_address = SCB->VTOR;

        pm_soc_clk_set_busclk(1, 0, 0, 0);
        break;
    default:
        /* this should not happen */
        break;
    }
}
