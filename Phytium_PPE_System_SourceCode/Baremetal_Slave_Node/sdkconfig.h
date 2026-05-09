#ifndef SDK_CONFIG_H__
#define SDK_CONFIG_H__

/* Project Configuration */

/* Baremetal Configuration */

#define CONFIG_TARGET_NAME "openamp_core0"
/* end of Baremetal Configuration */
#define CONFIG_OPENAMP_TRACE_DEBUG
/* end of Project Configuration */
#define CONFIG_USE_BAREMETAL

/* Arch configuration */

#define CONFIG_TARGET_ARMv8
#define CONFIG_ARCH_NAME "armv8"

/* Arm architecture configuration */

#define CONFIG_ARCH_ARMV8_AARCH64
/* CONFIG_ARCH_ARMV8_AARCH32 is not set */

/* Compiler configuration */

#define CONFIG_ARM_GCC_SELECT
/* CONFIG_ARM_CLANG_SELECT is not set */
#define CONFIG_TOOLCHAIN_NAME "gcc"
#define CONFIG_TARGET_ARMV8_AARCH64
#define CONFIG_ARCH_EXECUTION_STATE "aarch64"
#define CONFIG_ARM_NEON
#define CONFIG_ARM_CRC
#define CONFIG_ARM_CRYPTO
#define CONFIG_ARM_FLOAT_POINT
/* CONFIG_GCC_CODE_MODEL_TINY is not set */
#define CONFIG_GCC_CODE_MODEL_SMALL
/* CONFIG_GCC_CODE_MODEL_LARGE is not set */
/* end of Compiler configuration */
/* CONFIG_BOOT_WITH_FLUSH_CACHE is not set */
/* CONFIG_MMU_DEBUG_PRINTS is not set */
/* end of Arm architecture configuration */

/* multi-core system deployment framework */

/* CONFIG_USE_MSDF is not set */
/* end of multi-core system deployment framework */
#define CONFIG_MMU_PAGE_SIZE_4K
/* CONFIG_MMU_PAGE_SIZE_16K is not set */
/* CONFIG_MMU_PAGE_SIZE_64K is not set */
#define CONFIG_MMU_PAGE_SIZE 0x1000
/* CONFIG_USE_EL2 is not set */
#define CONFIG_MAX_XLAT_TABLES 256
/* CONFIG_ENABLE_GIC_ITS is not set */
/* CONFIG_ENABLE_GDB_STUB is not set */
/* end of Arch configuration */

/* Soc configuration */

#define CONFIG_TARGET_PE2204
/* CONFIG_TARGET_PE2202 is not set */
/* CONFIG_TARGET_PE2201 is not set */
/* CONFIG_TARGET_PD1904 is not set */
/* CONFIG_TARGET_PD2008 is not set */
/* CONFIG_TARGET_PD2308 is not set */
/* CONFIG_TARGET_PS2316 is not set */
/* CONFIG_TARGET_PD2408 is not set */
/* CONFIG_TARGET_QEMU_VIRT is not set */
#define CONFIG_SOC_NAME "pe220x"
#define CONFIG_TARGET_TYPE_NAME "pe2204"
#define CONFIG_SOC_CORE_NUM 4
#define CONFIG_F32BIT_MEMORY_ADDRESS 0x80000000
#define CONFIG_F32BIT_MEMORY_LENGTH 0x80000000
#define CONFIG_F64BIT_MEMORY_ADDRESS 0x2000000000
#define CONFIG_F64BIT_MEMORY_LENGTH 0x800000000
#define CONFIG_TARGET_PE220X
#define CONFIG_DEFAULT_DEBUG_PRINT_UART1
/* CONFIG_DEFAULT_DEBUG_PRINT_UART0 is not set */
/* CONFIG_DEFAULT_DEBUG_PRINT_UART2 is not set */
#define CONFIG_UART_INIT_NOT_CLR_INTR
/* end of Soc configuration */

/* Board Configuration */

#define CONFIG_BOARD_NAME "phytiumpi"
/* CONFIG_USE_SPI_IOPAD is not set */
/* CONFIG_USE_GPIO_IOPAD is not set */
/* CONFIG_USE_CAN_IOPAD is not set */
/* CONFIG_USE_QSPI_IOPAD is not set */
/* CONFIG_USE_PWM_IOPAD is not set */
/* CONFIG_USE_MIO_IOPAD is not set */
/* CONFIG_USE_TACHO_IOPAD is not set */
/* CONFIG_USE_UART_IOPAD is not set */
/* CONFIG_USE_THIRD_PARTY_IOPAD is not set */
/* CONFIG_E2000Q_DEMO_BOARD is not set */
#define CONFIG_PHYTIUMPI_FIREFLY_BOARD

/* IO mux configuration when board start up */

/* end of IO mux configuration when board start up */
/* CONFIG_CUS_DEMO_BOARD is not set */

/* Build project name */

/* end of Build project name */
/* end of Board Configuration */

/* Sdk common configuration */

#define CONFIG_ELOG_LINE_BUF_SIZE 0x100
/* CONFIG_LOG_VERBOS is not set */
/* CONFIG_LOG_DEBUG is not set */
#define CONFIG_LOG_INFO
/* CONFIG_LOG_WARN is not set */
/* CONFIG_LOG_ERROR is not set */
/* CONFIG_LOG_NONE is not set */
/* CONFIG_LOG_EXTRA_INFO is not set */
#define CONFIG_LOG_DISPALY_CORE_NUM
/* CONFIG_BOOTUP_DEBUG_PRINTS is not set */
#define CONFIG_USE_NS_GTIMER
/* CONFIG_USE_VIRTUAL_GTIMER is not set */
#define CONFIG_SLEEP_USE_NS_GTIMER
/* CONFIG_SLEEP_USE_VIRTUAL_GTIMER is not set */
#define CONFIG_USE_DEFAULT_INTERRUPT_CONFIG
/* CONFIG_INTERRUPT_ROLE_MASTER is not set */
#define CONFIG_INTERRUPT_ROLE_SLAVE
/* CONFIG_INTERRUPT_ROLE_NONE is not set */
/* end of Sdk common configuration */

/* Drivers configuration */

#define CONFIG_USE_IOMUX
/* CONFIG_ENABLE_IOCTRL is not set */
#define CONFIG_ENABLE_IOPAD
/* CONFIG_USE_SPI is not set */
/* CONFIG_USE_QSPI is not set */
#define CONFIG_USE_SERIAL

/* Usart Configuration */

#define CONFIG_ENABLE_Pl011_UART
/* end of Usart Configuration */
#define CONFIG_USE_GPIO
#define CONFIG_ENABLE_FGPIO
/* CONFIG_USE_ETH is not set */
/* CONFIG_USE_CAN is not set */
/* CONFIG_USE_I2C is not set */
#define CONFIG_USE_TIMER

/* Hardware Timer Configuration */

#define CONFIG_ENABLE_TIMER_TACHO
/* end of Hardware Timer Configuration */
/* CONFIG_USE_MIO is not set */
/* CONFIG_USE_SDMMC is not set */
/* CONFIG_USE_PCIE is not set */
/* CONFIG_USE_WDT is not set */
/* CONFIG_USE_DMA is not set */
/* CONFIG_USE_NAND is not set */
/* CONFIG_USE_RTC is not set */
/* CONFIG_USE_SATA is not set */
/* CONFIG_USE_ADC is not set */
/* CONFIG_USE_PWM is not set */
#define CONFIG_USE_IPC
/* CONFIG_ENABLE_FSEMAPHORE is not set */
/* CONFIG_USE_MEDIA is not set */
#define CONFIG_USE_SCMI_MHU

/* Scmi Configuration */

#define CONFIG_ENABLE_SCMI_MHU
/* end of Scmi Configuration */
/* CONFIG_USE_I2S is not set */
/* CONFIG_USE_I3C is not set */
/* CONFIG_USE_MSG is not set */
/* CONFIG_USE_DEVICE is not set */
/* end of Drivers configuration */

/* Third-party configuration */

/* CONFIG_USE_GMSSL is not set */
/* CONFIG_USE_MBEDTLS is not set */
/* CONFIG_USE_LWIP is not set */
/* CONFIG_USE_LETTER_SHELL is not set */
#define CONFIG_USE_AMP
#define CONFIG_USE_LIBMETAL

/* OpenAmp */

#define CONFIG_USE_OPENAMP
#define CONFIG_USE_OPENAMP_IPI
#define CONFIG_SKIP_SHBUF_IO_WRITE
#define CONFIG_USE_MASTER_VRING_DEFINE
#define CONFIG_USE_CACHE_COHERENCY
/* end of OpenAmp */
/* CONFIG_USE_YMODEM is not set */
/* CONFIG_USE_SFUD is not set */
/* CONFIG_USE_FATFS_0_1_4 is not set */
/* CONFIG_USE_SPIFFS is not set */
/* CONFIG_USE_LITTLE_FS is not set */
/* CONFIG_USE_LVGL is not set */
/* CONFIG_USE_FREEMODBUS is not set */
/* CONFIG_USE_FSL_SDMMC is not set */
/* CONFIG_USE_MICROPYTHON is not set */
/* CONFIG_USE_TINYMAIX is not set */
/* CONFIG_USE_CHERRY_USB is not set */
/* CONFIG_USE_CMSIS is not set */
/* CONFIG_USE_ACPICA is not set */
/* CONFIG_USE_CJSON is not set */
/* CONFIG_USE_VFS is not set */
/* end of Third-party configuration */

/* Build setup */

#define CONFIG_CHECK_DEPS
/* CONFIG_OUTPUT_BINARY is not set */

/* Optimization options */

/* CONFIG_DEBUG_NOOPT is not set */
/* CONFIG_DEBUG_CUSTOMOPT is not set */
#define CONFIG_DEBUG_FULLOPT
#define CONFIG_DEBUG_OPT_UNUSED_SECTIONS
#define CONFIG_DEBUG_LINK_MAP
/* CONFIG_CCACHE is not set */
/* CONFIG_ARCH_COVERAGE is not set */
/* CONFIG_LTO_FULL is not set */
/* end of Optimization options */

/* Debug options */

/* CONFIG_DEBUG_ENABLE_ALL_WARNING is not set */
/* CONFIG_WALL_WARNING_ERROR is not set */
/* CONFIG_STRICT_PROTOTYPES is not set */
#define CONFIG_DEBUG_SYMBOLS
/* CONFIG_FRAME_POINTER is not set */
/* CONFIG_OUTPUT_ASM_DIS is not set */
/* CONFIG_ENABLE_WSHADOW is not set */
/* CONFIG_ENABLE_WUNDEF is not set */
#define CONFIG_DOWNGRADE_DIAG_WARNING
/* end of Debug options */

/* Lib */

#define CONFIG_USE_COMPILE_CHAIN
/* CONFIG_USE_NEWLIB is not set */
/* CONFIG_USE_USER_DEFINED is not set */
/* end of Lib */
/* CONFIG_ENABLE_CXX is not set */

/* Linker Options */

#define CONFIG_DEFAULT_LINKER_SCRIPT
/* CONFIG_USER_DEFINED_LD is not set */
#define CONFIG_IMAGE_LOAD_ADDRESS 0xb0100000
#define CONFIG_IMAGE_MAX_LENGTH 0x1000000
#define CONFIG_HEAP_SIZE 1
#define CONFIG_STACK_SIZE 0x1000
/* end of Linker Options */
/* end of Build setup */

#endif
