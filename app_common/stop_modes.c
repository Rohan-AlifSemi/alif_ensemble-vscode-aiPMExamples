#include <alif.h>
#include <pm_soc_pwr.h>
#include <se_services_port.h>
#include <services_lib_api.h>
#include <soc_vbat.h>

#if defined(M55_HP)
#define CLOCK_FREQUENCY_CPU         CLOCK_FREQUENCY_400MHZ
#elif defined(M55_HE)
#define CLOCK_FREQUENCY_CPU         CLOCK_FREQUENCY_160MHZ
#endif

void execute_stop_mode_usecase(uint32_t mode_number)
{
    mode_number++;

#if defined(M55_HE)
    /* STOP_3 through STOP_5 will have 4kB Backup SRAM disabled */
    if (mode_number > 2) {
        pm_soc_retain_backup_ram(0);
    }
#endif

    /* go to subsystem off */
    while(1) 
    {
        __disable_irq();
        pm_core_enter_deep_sleep_request_subsys_off();
        __enable_irq();
    }
}

void configure_stop_mode_profiles(uint32_t mode_number,
        run_profile_t *runp, off_profile_t *offp)
{
    /* BACKUP4K_MASK must be enabled for the HP core for this demo
     * but it will be disabled by the HE core at the last second */
    runp->memory_blocks = MRAM_MASK | BACKUP4K_MASK;
    offp->memory_blocks = MRAM_MASK | BACKUP4K_MASK;

    runp->vdd_ioflex_3V3 = IOFLEX_LEVEL_1V8;
    offp->vdd_ioflex_3V3 = IOFLEX_LEVEL_1V8;

    runp->run_clk_src = CLK_SRC_HFRC;
    runp->cpu_clk_freq = CLOCK_FREQUENCY_76_8_RC_MHZ;
    runp->scaled_clk_freq = SCALED_FREQ_RC_STDBY_0_6_MHZ;

    runp->dcdc_mode = DCDC_MODE_PWM;
    runp->dcdc_voltage = DCDC_VOUT_0825;

    runp->power_domains = PD_SSE700_AON_MASK;
    offp->power_domains = PD_VBAT_AON_MASK;

    offp->vtor_address = SCB->VTOR;

    switch(mode_number+1) {
    case 1:
        /* requires external 32k XTAL to be installed */
        runp->aon_clk_src       = CLK_SRC_LFXO;
        offp->aon_clk_src       = CLK_SRC_LFXO;
        offp->memory_blocks    |= SRAM4_1_MASK | SRAM4_2_MASK | SRAM5_1_MASK | SRAM5_2_MASK | BACKUP4K_MASK;
        VBATALL->RTC_CLK_EN    |= VBATALL_RTC_CLK_EN_Msk;
        VBATSEC->VBAT_ANA_REG1 |= VBATSEC_VBAT_ANA_REG1_LPCMP_EN_Msk;
        VBATSEC->VBAT_ANA_REG3 |= VBATSEC_VBAT_ANA_REG3_BOD_EN_Msk;
        break;
    case 2:
        /* requires external 32k XTAL to be installed */
        runp->aon_clk_src       = CLK_SRC_LFXO;
        offp->aon_clk_src       = CLK_SRC_LFXO;
        offp->memory_blocks    |= BACKUP4K_MASK;
        VBATALL->RTC_CLK_EN    |= VBATALL_RTC_CLK_EN_Msk;
        VBATSEC->VBAT_ANA_REG1 |= VBATSEC_VBAT_ANA_REG1_LPCMP_EN_Msk;
        VBATSEC->VBAT_ANA_REG3 |= VBATSEC_VBAT_ANA_REG3_BOD_EN_Msk;
        break;
    case 3:
        /* requires external 32k XTAL to be installed */
        runp->aon_clk_src       = CLK_SRC_LFXO;
        offp->aon_clk_src       = CLK_SRC_LFXO;
        VBATALL->RTC_CLK_EN    |= VBATALL_RTC_CLK_EN_Msk;
        VBATSEC->VBAT_ANA_REG1 |= VBATSEC_VBAT_ANA_REG1_LPCMP_EN_Msk;
        VBATSEC->VBAT_ANA_REG3 |= VBATSEC_VBAT_ANA_REG3_BOD_EN_Msk;
        break;
    case 4:
        /* requires external 32k XTAL to be installed */
        runp->aon_clk_src       = CLK_SRC_LFXO;
        offp->aon_clk_src       = CLK_SRC_LFXO;
        VBATALL->RTC_CLK_EN    |= VBATALL_RTC_CLK_EN_Msk;
        VBATSEC->VBAT_ANA_REG1 &= ~VBATSEC_VBAT_ANA_REG1_LPCMP_EN_Msk;
        VBATSEC->VBAT_ANA_REG3 &= ~VBATSEC_VBAT_ANA_REG3_BOD_EN_Msk;
        break;
    case 5:
        runp->aon_clk_src       = CLK_SRC_LFRC;
        offp->aon_clk_src       = CLK_SRC_LFRC;
        VBATALL->RTC_CLK_EN    &= ~VBATALL_RTC_CLK_EN_Msk;
        VBATSEC->VBAT_ANA_REG1 &= ~VBATSEC_VBAT_ANA_REG1_LPCMP_EN_Msk;
        VBATSEC->VBAT_ANA_REG3 &= ~VBATSEC_VBAT_ANA_REG3_BOD_EN_Msk;
        break;
#if defined(ENSEMBLE_SOC_GEN2)
    case 6:
        /* requires external 32k XTAL to be installed */
        runp->aon_clk_src       = CLK_SRC_LFXO;
        offp->aon_clk_src       = CLK_SRC_LFXO;
        offp->memory_blocks    |= SRAM0_1_RET_MASK | SRAM0_2_RET_MASK | SRAM0_3_RET_MASK | SRAM0_4_RET_MASK | SRAM1_RET_MASK;
        offp->memory_blocks    |= SRAM4_1_MASK | SRAM4_2_MASK | SRAM5_1_MASK | SRAM5_2_MASK | BACKUP4K_MASK;
        VBATALL->RTC_CLK_EN    |= VBATALL_RTC_CLK_EN_Msk;
        VBATSEC->VBAT_ANA_REG1 |= VBATSEC_VBAT_ANA_REG1_LPCMP_EN_Msk;
        VBATSEC->VBAT_ANA_REG3 |= VBATSEC_VBAT_ANA_REG3_BOD_EN_Msk;
        break;
#endif
    default:
        /* this should not happen */
        break;
    }
}
