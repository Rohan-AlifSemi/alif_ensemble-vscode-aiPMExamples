/* standard lib */
#include <inttypes.h>
#include <stdio.h>

/* alif cmsis & services */
#include <alif.h>
#include <se_services_port.h>
#include <sys_clocks.h>

/* alif cmsis drivers */
#include <lptimer.h>
#include <pinconf.h>
#include <uart.h>

/* pm soc drivers */
#include <drv_bkram.h>
#include <drv_cgu.h>
#include <drv_mhu.h>
#include <pm_soc_clk.h>
#include <soc_aon.h>
#include <soc_hostbase.h>
#include <soc_vbat.h>

#include <RTE_Components.h>
#if defined(RTE_CMSIS_Compiler_STDIN) || defined(RTE_CMSIS_Compiler_STDOUT)
#define _UART_BASE_(n) UART##n##_BASE
#define UART_BASE(n) _UART_BASE_(n)
#define _UART_CLK_SRC_(n)   RTE_UART##n##_CLK_SOURCE
#define UART_CLK_SRC(n)     _UART_CLK_SRC_(n)
#include <retarget_init.h>
#include <retarget_config.h>
#endif

#include <power_modes_common.h>

#define LPT_CH  0
volatile uint32_t lpt_irq;

#define MHU_VAL 0x1234
volatile uint32_t mhu_rx_value;

volatile uint32_t debug_loop;
volatile uint32_t ms_ticks;
void SysTick_Handler (void) { ms_ticks++; }
void delay_ms (uint32_t msec) { msec += ms_ticks; while(ms_ticks < msec) __WFI(); }
extern int32_t get_int_input();

static bool GetPendingIRQ();
static void PrintPendingIRQ();
static void SERVICES_ret(uint32_t ret);
static void SERVICES_response(uint32_t response);

static void uart_init();
static void uart_update();
static void uart_deinit();

static char pwr_modes_str[5][20] =
{
    "\t1. GO_MODE\r\n",
    "\t2. READY_MODE\r\n",
    "\t3. IDLE_MODE\r\n",
    "\t4. STANDBY_MODE\r\n",
    "\t5. STOP_MODE\r\n",
};

static char pwr_lvl_str[5][80] =
{
    "\t1. GO_1\r\n\t2. GO_2\r\n\t3. GO_3\r\n\t4. GO_4\r\n\t5. GO_5\r\n",
    "\t1. READY_1\r\n\t2. READY_2\r\n",
    "\t1. IDLE_1\r\n\t2. IDLE_2\r\n",
    "\t1. STANDBY_1\r\n\t2. STANDBY_2\r\n",
#if defined(ENSEMBLE_SOC_GEN2)
    "\t1. STOP_1\r\n\t2. STOP_2\r\n\t3. STOP_3\r\n\t4. STOP_4\r\n\t5. STOP_5\r\n\t6. STOP_0\r\n",
#else
    "\t1. STOP_1\r\n\t2. STOP_2\r\n\t3. STOP_3\r\n\t4. STOP_4\r\n\t5. STOP_5\r\n",
#endif
};

#if defined(ENSEMBLE_SOC_GEN2)
static uint32_t pwr_lvl_cnt[5] = {5, 2, 2, 2, 6};
#else
static uint32_t pwr_lvl_cnt[5] = {5, 2, 2, 2, 5};
#endif

static void print_pwr_modes_str()
{
    uint32_t i;

    printf("Availble Power Mode types\r\n");

    for(i = 0; i < 5; i++)
    {
        printf("%s", &pwr_modes_str[i][0]);
    }
    printf("\r\n");
}

static void print_pwr_lvl_str(uint32_t i)
{
    printf("Availble Power Mode levels\r\n");
    printf("%s", &pwr_lvl_str[i][0]);
    printf("\t%" PRIu32 ". Cancel\r\n", pwr_lvl_cnt[i] + 1);
    printf("\r\n");
}

int main (void)
{
    ms_ticks = 0;
    pm_core_clk_update();
    SysTick_Config(SystemCoreClock/1000);
    uart_init();

    printf("===================================================\r\n");
    printf("        Welcome to the Power Modes Demo App\r\n");
    printf("===================================================\r\n\n");

    /* initialize the SE SERVICES */
    uint32_t ret, service_resp;
    se_services_port_init();

    run_profile_t default_runp = {0};
    off_profile_t default_offp = {0};

    /* change to CLK_SRC_LFRC if 32k XTAL not installed */
    default_runp.aon_clk_src = CLK_SRC_LFXO;
    default_offp.aon_clk_src = CLK_SRC_LFXO;

    /* change to CLK_SRC_HFRC if 38.4MHz XTAL not installed */
    default_runp.run_clk_src = CLK_SRC_PLL;

    /* change to CLOCK_FREQUENCY_76_8_RC_MHZ if 38.4MHz XTAL not installed */
    default_runp.cpu_clk_freq = CLOCK_FREQUENCY_160MHZ;

    /* this is the frequency of the MCU when HFXO and PLL are disabled */
    default_runp.scaled_clk_freq = SCALED_FREQ_RC_ACTIVE_76_8_MHZ;

    /* the enum or an integer value 750-850 is accepted for dcdc_voltage */
    default_runp.dcdc_mode = DCDC_MODE_PWM;
    default_offp.dcdc_mode = DCDC_MODE_PWM;
    default_runp.dcdc_voltage = DCDC_VOUT_0825;
    default_offp.dcdc_voltage = DCDC_VOUT_0825;

    default_runp.memory_blocks = MRAM_MASK | BACKUP4K_MASK;
    default_offp.memory_blocks = MRAM_MASK | BACKUP4K_MASK;

    default_runp.power_domains = PD_DBSS_MASK | PD_SYST_MASK;
    default_offp.power_domains = PD_DBSS_MASK | PD_SYST_MASK;

    /* adjust this based on VDD_IO_FLEX voltage in your schematic */
    default_runp.vdd_ioflex_3V3 = IOFLEX_LEVEL_1V8;
    default_offp.vdd_ioflex_3V3 = IOFLEX_LEVEL_1V8;

    ret = SERVICES_set_run_cfg(se_services_s_handle, &default_runp, &service_resp);
    pm_core_clk_update();
    SysTick_Config(SystemCoreClock/1000);
    uart_update();
    if ((ret != 0) || (service_resp != 0)) {
        printf("\r\nERROR in SERVICES_set_run_cfg()\r\n");
        SERVICES_ret(ret);
        SERVICES_response(service_resp);
        printf("\r\n");
    }

    ret = SERVICES_set_off_cfg(se_services_s_handle, &default_offp, &service_resp);
    if ((ret != 0) || (service_resp != 0)) {
        printf("\r\nERROR in SERVICES_set_off_cfg()\r\n");
        SERVICES_ret(ret);
        SERVICES_response(service_resp);
        printf("\r\n");
    }

    uint32_t power_mode_type, power_mode_level;
    while(1) {
        print_pwr_modes_str();
        printf("Enter Power Mode type to be run:\r\n");
        printf("> ");
        power_mode_type = get_int_input() - 1;
        printf("\r\n\n");

        if (power_mode_type < 5) {
            print_pwr_lvl_str(power_mode_type);
            printf("Enter Power Mode level to be run:\r\n");
            printf("> ");
            power_mode_level = get_int_input() - 1;
            printf("\r\n\n");

            if (power_mode_level < pwr_lvl_cnt[power_mode_type]) {
                break;
            }
            else {
                printf("Returning to main menu\r\n\n");
            }
        }
    }

    printf("Starting Power Modes Demo\r\n\n");

    /* if the HP core is present: boot and notify the RTSS-HP core, then wait for a response */
#if !defined(ENSEMBLE_SOC_E1C)
    printf("RTSS-HE will power on the RTSS-HP and send a message\r\n\n");

    /* Clear the Backup RAM */
    uint32_t bk_data = 0;
    for (uint32_t i = 0; i < 2; i++) {
        bk_ram_wr(&bk_data, i);
    }

    /* write data to Backup RAM to be read by the RTSS-HP core */
    bk_ram_wr(&power_mode_type, 0);
    bk_ram_wr(&power_mode_level, 1);

    const uint8_t image_id[8] = "HP_MRAM";
    ret = SERVICES_boot_process_toc_entry(se_services_s_handle, image_id, &service_resp);
    if ((ret != 0) || (service_resp != 0)) {
        printf("\r\nERROR in SERVICES_boot_process_toc_entry()\r\n");
        SERVICES_ret(ret);
        SERVICES_response(service_resp);
    }
    else
    {
        /* enable the IRQ and let the handler update mhu_rx_value */
        mhu_rx_value = 0;
        NVIC_EnableIRQ(MHU_M55HP_M55HE_0_RX_IRQ_IRQn);

        /* interrupt the HP core via MHU */
        MHU_SENDER_Set(RTSS_TX_MHU0_BASE, 0, MHU_VAL);

        /* go to sleep (WFI) until HP responds, or timeout */
        uint32_t msec_delay = ms_ticks + 100;
        while ((mhu_rx_value != MHU_VAL) && (ms_ticks < msec_delay)) {
            __disable_irq();
            pm_core_enter_normal_sleep();
            __enable_irq();
        }

        if (mhu_rx_value) {
            printf("RTSS-HE received a reply from RTSS-HP\r\n\n");
        }
        else {
            HOSTBASE->BSYS_PWR_REQ |= HOSTBASE_BSYS_PWR_REQ_DBGTOP_PWR_REQ_Msk;
            printf("RTSS-HE continuing without reply from RTSS-HP\r\n\n");
            // ret = SERVICES_boot_reset_cpu(se_services_s_handle, EXTSYS_0, &service_resp);
            // if ((ret != 0) || (service_resp != 0)) {
            //     printf("\r\nERROR in SERVICES_boot_reset_cpu()\r\n");
            //     SERVICES_ret(ret);
            //     SERVICES_response(service_resp);
            // }
        }
    }
#endif

    run_profile_t runp = {0};
    off_profile_t offp = {0};

    switch(power_mode_type)
    {
    case 0:
        configure_go_mode_profiles(power_mode_level, &runp, &offp);
        break;
    case 1:
        configure_ready_mode_profiles(power_mode_level, &runp, &offp);
        break;
    case 2:
        configure_idle_mode_profiles(power_mode_level, &runp, &offp);
        break;
    case 3:
        configure_standby_mode_profiles(power_mode_level, &runp, &offp);
        break;
    case 4:
        configure_stop_mode_profiles(power_mode_level, &runp, &offp);
        break;
    default:
        break;
    }

    ret = SERVICES_set_run_cfg(se_services_s_handle, &runp, &service_resp);
    pm_core_clk_update();
    SysTick_Config(SystemCoreClock/1000);
    uart_update();
    if ((ret != 0) || (service_resp != 0)) {
        printf("\r\nERROR in SERVICES_set_run_cfg()\r\n");
        SERVICES_ret(ret);
        SERVICES_response(service_resp);
        printf("\r\n");
    }

    ret = SERVICES_set_off_cfg(se_services_s_handle, &offp, &service_resp);
    if ((ret != 0) || (service_resp != 0)) {
        printf("\r\nERROR in SERVICES_set_off_cfg()\r\n");
        SERVICES_ret(ret);
        SERVICES_response(service_resp);
        printf("\r\n");
    }

    /* synchronize HOSTBASE->BSYS_PWR_REQ register with run profile */
    if (runp.power_domains & PD_SYST_MASK) {
        HOSTBASE->BSYS_PWR_REQ = HOSTBASE_BSYS_PWR_REQ_SYSTOP_PWR_REQ_FUNC_RET;
    }
    else {
        HOSTBASE->BSYS_PWR_REQ = HOSTBASE_BSYS_PWR_REQ_SYSTOP_PWR_REQ_OFF;
    }

    switch(power_mode_type)
    {
    case 0:
        execute_go_mode_usecase(power_mode_level);
        break;
    case 1:
        execute_ready_mode_usecase(power_mode_level);
        break;
    case 2:
        delay_ms(10);
        ret = SERVICES_power_se_sleep_req(se_services_s_handle, 0, &service_resp);
        if ((ret != 0) || (service_resp != 0)) {
            printf("\r\nERROR in SERVICES_boot_reset_cpu()\r\n");
            SERVICES_ret(ret);
            SERVICES_response(service_resp);
        }
        delay_ms(10);
        execute_idle_mode_usecase(power_mode_level);
        break;
    case 3:
        delay_ms(10);
        ret = SERVICES_power_se_sleep_req(se_services_s_handle, 0, &service_resp);
        if ((ret != 0) || (service_resp != 0)) {
            printf("\r\nERROR in SERVICES_boot_reset_cpu()\r\n");
            SERVICES_ret(ret);
            SERVICES_response(service_resp);
        }
        delay_ms(10);
        execute_standby_mode_usecase(power_mode_level);
        break;
    case 4:
        delay_ms(10);
        execute_stop_mode_usecase(power_mode_level);
        break;
    default:
        break;
    }

    return 0;

    (void) AONALL;
    (void) AONSEC;
    (void) CGU;
    (void) CLKCTL_PER_MST;
    (void) CLKCTL_PER_SLV;
    (void) M55HE_CFG;
    (void) PINMUX;
    (void) SDMMC;
    (void) VBATALL;
    (void) VBATSEC;
    (void) VBATSEC2;
}

static bool GetPendingIRQ()
{
    uint32_t wic_pending = 0;
    wic_pending |= NVIC->ISPR[0];
    wic_pending |= NVIC->ISPR[1];

    /* nothing to do if IRQs 0-63 are clear */
    if (wic_pending == 0) return false;

    /* only LPTIMERx_IRQ_IRQn (x = LPT_CH) should wake the HE core */
    if (NVIC_GetPendingIRQ(LPTIMER0_IRQ_IRQn + LPT_CH)) {
        return true;
    }

    return false;
}

static void PrintPendingIRQ()
{
    /* Note: IRQ lines are shared in this multicore system,
     * you may see pending IRQs not meant for this core. */
    for (uint32_t i = 0; i < 64; i++) {
        if (NVIC_GetPendingIRQ(i)) {
            printf("IRQ%" PRIu32 " is pending\r\n", i);
        }
    }
}

void SERVICES_ret(uint32_t ret)
{
    printf("SERVICES function ret = %" PRIu32 "\r\n", ret);
}

void SERVICES_response(uint32_t response)
{
    printf("SERVICES function response = %" PRIu32 "\r\n", response);
}

static void uart_init()
{
#if PRINTF_UART_CONSOLE != LP
#if UART_CLK_SRC(PRINTF_UART_CONSOLE) == 0
    CGU_clock_enable_clk38p4M(true);
    pm_soc_clk_get_hfosc();
#else
    pm_soc_clk_get_busclk();
#endif
#endif
#if defined(RTE_CMSIS_Compiler_STDIN_Custom)
    stdin_init();
#endif
#if defined(RTE_CMSIS_Compiler_STDOUT_Custom)
    stdout_init();
#endif
}

/* call this function after making clock tree changes */
static void uart_update()
{
#if defined(RTE_CMSIS_Compiler_STDIN_Custom) || defined(RTE_CMSIS_Compiler_STDOUT_Custom)
#if PRINTF_UART_CONSOLE == LP
    uart_set_baudrate((UART_Type*)LPUART_BASE, SystemCoreClock, PRINTF_UART_CONSOLE_BAUD_RATE);
#elif UART_CLK_SRC(PRINTF_UART_CONSOLE) == 0
    pm_soc_clk_get_hfosc();
    uart_set_baudrate((UART_Type*)UART_BASE(PRINTF_UART_CONSOLE), SystemHFOSCClock, PRINTF_UART_CONSOLE_BAUD_RATE);
#else
    pm_soc_clk_get_busclk();
    uart_set_baudrate((UART_Type*)UART_BASE(PRINTF_UART_CONSOLE), SystemAPBClock, PRINTF_UART_CONSOLE_BAUD_RATE);
#endif
#endif
}

static void uart_deinit()
{
#if defined(RTE_CMSIS_Compiler_STDIN_Custom)
    pinconf_set(PRINTF_UART_CONSOLE_RX_PORT_NUM, PRINTF_UART_CONSOLE_RX_PIN,
        0, PADCTRL_DRIVER_DISABLED_PULL_UP);
#endif
#if defined(RTE_CMSIS_Compiler_STDOUT_Custom)
    pinconf_set(PRINTF_UART_CONSOLE_TX_PORT_NUM, PRINTF_UART_CONSOLE_TX_PIN,
        0, PADCTRL_DRIVER_DISABLED_PULL_UP);
#endif
}

void LPTIMER0_IRQHandler()
{
    LPTIMER_Type *lptimer = (LPTIMER_Type *) LPTIMER_BASE;
    lptimer_clear_interrupt(lptimer, 0);
    lpt_irq = 1;

    uint32_t count;
    bk_ram_rd(&count, BKRAM_INDEX_LPT0_COUNT);
    count++;
    bk_ram_wr(&count, BKRAM_INDEX_LPT0_COUNT);
}

void LPTIMER1_IRQHandler()
{
    LPTIMER_Type *lptimer = (LPTIMER_Type *) LPTIMER_BASE;
    lptimer_clear_interrupt(lptimer, 1);
    lpt_irq = 1;

    uint32_t count;
    bk_ram_rd(&count, BKRAM_INDEX_LPT1_COUNT);
    count++;
    bk_ram_wr(&count, BKRAM_INDEX_LPT1_COUNT);
}

void MHU_RTSS_S_TX_IRQHandler()
{
    MHU_SENDER_regs *MHU = (MHU_SENDER_regs *) RTSS_TX_MHU0_BASE;
    uint32_t int_st = MHU->INT_ST;
    MHU->INT_CLR = int_st;
}

void MHU_RTSS_S_RX_IRQHandler()
{
    MHU_RECEIVER_regs *MHU = (MHU_RECEIVER_regs *) RTSS_RX_MHU0_BASE;
    uint32_t int_st = MHU->INT_ST;

    uint32_t check_val;
    MHU_RECEIVER_Read(RTSS_RX_MHU0_BASE, 0, &check_val);
    MHU_RECEIVER_Clear(RTSS_RX_MHU0_BASE, 0, check_val);
    mhu_rx_value = check_val;

    MHU->INT_CLR = int_st;
}