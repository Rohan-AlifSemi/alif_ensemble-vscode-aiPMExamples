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

#define MHU_VAL 0x1234
volatile uint32_t mhu_rx_value;

volatile uint32_t debug_loop;
volatile uint32_t ms_ticks;
void SysTick_Handler (void) { ms_ticks++; }
void delay_ms (uint32_t msec) { msec += ms_ticks; while(ms_ticks < msec) __WFI(); }

static bool GetPendingIRQ();
static void PrintPendingIRQ();
static void SERVICES_ret(uint32_t ret);
static void SERVICES_response(uint32_t response);

static void uart_init();
static void uart_update();
static void uart_deinit();

static char pwr_modes_str[5][8] =
{
    "GO",
    "READY",
    "IDLE",
    "STANDBY",
    "STOP",
};

int main (void)
{
    ms_ticks = 0;
    pm_core_clk_update();
    SysTick_Config(SystemCoreClock/1000);
    // uart_init();

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
    default_runp.cpu_clk_freq = CLOCK_FREQUENCY_400MHZ;

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
    // uart_update();
    // if ((ret != 0) || (service_resp != 0)) {
    //     printf("\r\nERROR in SERVICES_set_run_cfg()\r\n");
    //     SERVICES_ret(ret);
    //     SERVICES_response(service_resp);
    //     printf("\r\n");
    // }

    ret = SERVICES_set_off_cfg(se_services_s_handle, &default_offp, &service_resp);
    // if ((ret != 0) || (service_resp != 0)) {
    //     printf("\r\nERROR in SERVICES_set_off_cfg()\r\n");
    //     SERVICES_ret(ret);
    //     SERVICES_response(service_resp);
    //     printf("\r\n");
    // }

    /* something about the previous SERVICES API call prevents
     * the HP core from being able to shut down if this delay is removed */
    delay_ms(10);

    bool wake_event = GetPendingIRQ();
    if (wake_event == 0) {
        /* no pending work to do - power down the RTSS-HP */
        __disable_irq();
        pm_core_enter_deep_sleep_request_subsys_off();
        __enable_irq();
    }

    /* enable the IRQ and let the handler update mhu_rx_value */
    mhu_rx_value = 0;
    NVIC_EnableIRQ(MHU_M55HE_M55HP_0_RX_IRQ_IRQn);
    while(mhu_rx_value != MHU_VAL) {
        __disable_irq();
        pm_core_enter_deep_sleep();
        __enable_irq();
    }

    /* get data from Backup RAM written by the RTSS-HE core */
    uint32_t power_mode_type, power_mode_level;
    bk_ram_rd(&power_mode_type, 0);
    bk_ram_rd(&power_mode_level, 1);

    // printf("Executing Power Mode %s_%"PRIu32"\r\n\n", pwr_modes_str[power_mode_type], power_mode_level);

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

    // printf("HP applying RUN and OFF profiles\r\n");

    ret = SERVICES_set_run_cfg(se_services_s_handle, &runp, &service_resp);
    pm_core_clk_update();
    SysTick_Config(SystemCoreClock/1000);
    // uart_update();
    // if ((ret != 0) || (service_resp != 0)) {
    //     printf("\r\nERROR in SERVICES_set_run_cfg()\r\n");
    //     SERVICES_ret(ret);
    //     SERVICES_response(service_resp);
    //     printf("\r\n");
    // }

    ret = SERVICES_set_off_cfg(se_services_s_handle, &offp, &service_resp);
    // if ((ret != 0) || (service_resp != 0)) {
    //     printf("\r\nERROR in SERVICES_set_off_cfg()\r\n");
    //     SERVICES_ret(ret);
    //     SERVICES_response(service_resp);
    //     printf("\r\n");
    // }

    // printf("HP responding to HE core...\r\n");

    /* respond to the RTSS-HE core before continuing */
    MHU_SENDER_Set(RTSS_TX_MHU0_BASE, 0, MHU_VAL);

    switch(power_mode_type)
    {
    case 0:
        execute_go_mode_usecase(power_mode_level);
        break;
    case 1:
        execute_ready_mode_usecase(power_mode_level);
        break;
    case 2:
        execute_idle_mode_usecase(power_mode_level);
        break;
    case 3:
        execute_standby_mode_usecase(power_mode_level);
        break;
    case 4:
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

    /* only MHU_M55HE_M55HP_0_RX_IRQ should wake the HP core */
    if (NVIC_GetPendingIRQ(MHU_M55HE_M55HP_0_RX_IRQ_IRQn)) {
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
#if UART_CLK_SRC(PRINTF_UART_CONSOLE) == 0
    CGU_clock_enable_clk38p4M(true);
    pm_soc_clk_get_hfosc();
#else
    pm_soc_clk_get_busclk();
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
#if UART_CLK_SRC(PRINTF_UART_CONSOLE) == 0
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