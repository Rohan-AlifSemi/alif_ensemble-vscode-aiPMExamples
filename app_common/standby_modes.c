#include <alif.h>
#include <se_services_port.h>
#include <services_lib_api.h>
#include <soc_vbat.h>

#if defined(M55_HP)
#define CLOCK_FREQUENCY_CPU         CLOCK_FREQUENCY_400MHZ
#elif defined(M55_HE)
#define CLOCK_FREQUENCY_CPU         CLOCK_FREQUENCY_160MHZ
#endif

void execute_standby_mode_usecase(uint32_t mode_number)
{
    switch(mode_number+1) {
    case 1:
        /* go to subsystem off */
        while(1) 
        {
            __disable_irq();
            pm_core_enter_deep_sleep_request_subsys_off();
            __enable_irq();
        }

        break;
    case 2:
        /* go to subsystem off */
        while(1) 
        {
            __disable_irq();
#if defined(M55_HE)
            /* disable the MRAM LDO, save 100uA
             * note: code must be stored elsewhere */
        VBATSEC->VBAT_ANA_REG2 &= ~VBATSEC_VBAT_ANA_REG2_MRAM_LDO_EN_Msk;
#endif
            pm_core_enter_deep_sleep_request_subsys_off();
#if defined(M55_HE)
            /* enable the MRAM LDO, before serving IRQs */
        VBATSEC->VBAT_ANA_REG2 |= VBATSEC_VBAT_ANA_REG2_MRAM_LDO_EN_Msk;
#endif
            __enable_irq();
        }

        break;
    default:
        /* this should not happen */
        break;
    }
}

void configure_standby_mode_profiles(uint32_t mode_number,
        run_profile_t *runp, off_profile_t *offp)
{
    /* change to CLK_SRC_LFRC if 32k XTAL not installed */
    runp->aon_clk_src = CLK_SRC_LFXO;
    offp->aon_clk_src = CLK_SRC_LFXO;

    runp->memory_blocks = MRAM_MASK | BACKUP4K_MASK;
    offp->memory_blocks = MRAM_MASK | BACKUP4K_MASK;
    offp->memory_blocks |= SRAM4_1_MASK | SRAM4_2_MASK | SRAM5_1_MASK | SRAM5_2_MASK;

    runp->vdd_ioflex_3V3 = IOFLEX_LEVEL_1V8;
    offp->vdd_ioflex_3V3 = IOFLEX_LEVEL_1V8;

    runp->run_clk_src = CLK_SRC_HFRC;
    runp->cpu_clk_freq = CLOCK_FREQUENCY_76_8_RC_MHZ;
    runp->scaled_clk_freq = SCALED_FREQ_RC_STDBY_0_6_MHZ;

#if defined(M55_HP)
        runp->dcdc_mode = DCDC_MODE_PWM;
#else
        runp->dcdc_mode = DCDC_MODE_PFM_FORCED;
#endif
    runp->dcdc_voltage = 775;

    runp->power_domains = PD_SSE700_AON_MASK;
    offp->power_domains = PD_SSE700_AON_MASK;

    offp->vtor_address = SCB->VTOR;
}
