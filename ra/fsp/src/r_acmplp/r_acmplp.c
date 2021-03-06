/***********************************************************************************************************************
 * Copyright [2020] Renesas Electronics Corporation and/or its affiliates.  All Rights Reserved.
 *
 * This software and documentation are supplied by Renesas Electronics America Inc. and may only be used with products
 * of Renesas Electronics Corp. and its affiliates ("Renesas").  No other uses are authorized.  Renesas products are
 * sold pursuant to Renesas terms and conditions of sale.  Purchasers are solely responsible for the selection and use
 * of Renesas products and Renesas assumes no liability.  No license, express or implied, to any intellectual property
 * right is granted by Renesas. This software is protected under all applicable laws, including copyright laws. Renesas
 * reserves the right to change or discontinue this software and/or this documentation. THE SOFTWARE AND DOCUMENTATION
 * IS DELIVERED TO YOU "AS IS," AND RENESAS MAKES NO REPRESENTATIONS OR WARRANTIES, AND TO THE FULLEST EXTENT
 * PERMISSIBLE UNDER APPLICABLE LAW, DISCLAIMS ALL WARRANTIES, WHETHER EXPLICITLY OR IMPLICITLY, INCLUDING WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NONINFRINGEMENT, WITH RESPECT TO THE SOFTWARE OR
 * DOCUMENTATION.  RENESAS SHALL HAVE NO LIABILITY ARISING OUT OF ANY SECURITY VULNERABILITY OR BREACH.  TO THE MAXIMUM
 * EXTENT PERMITTED BY LAW, IN NO EVENT WILL RENESAS BE LIABLE TO YOU IN CONNECTION WITH THE SOFTWARE OR DOCUMENTATION
 * (OR ANY PERSON OR ENTITY CLAIMING RIGHTS DERIVED FROM YOU) FOR ANY LOSS, DAMAGES, OR CLAIMS WHATSOEVER, INCLUDING,
 * WITHOUT LIMITATION, ANY DIRECT, CONSEQUENTIAL, SPECIAL, INDIRECT, PUNITIVE, OR INCIDENTAL DAMAGES; ANY LOST PROFITS,
 * OTHER ECONOMIC DAMAGE, PROPERTY DAMAGE, OR PERSONAL INJURY; AND EVEN IF RENESAS HAS BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH LOSS, DAMAGES, CLAIMS OR COSTS.
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/

#include "bsp_api.h"
#include "r_acmplp_cfg.h"
#include "r_acmplp.h"

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define ACMPLP_OPEN                 (0x434d504CU)

/* Mask for clearing ACMPLP registers*/
#define ACMPLP_COMPMDR_CH0_CLEAR    (0xF1U)
#define ACMPLP_COMPFIR_CH0_CLEAR    (0xF0U)
#define ACMPLP_COMPOCR_CH0_CLEAR    (0xF9U)
#define ACMPLP_COMPMDR_CH1_CLEAR    (0x1FU)
#define ACMPLP_COMPFIR_CH1_CLEAR    (0x0FU)
#define ACMPLP_COMPOCR_CH1_CLEAR    (0x9FU)
#define ACMPLP_COMPSEL_CH0_CLEAR    (0xF0)
#define ACMPLP_COMPSEL_CH1_CLEAR    (0x0F)

/* Set Speed mode to high-speed*/
#define ACMPLP_CPMPOCR_SPDMD_SET    (0x80U)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
static void acmplp_hardware_initialize(acmplp_instance_ctrl_t * const p_instance_ctrl);

/***********************************************************************************************************************
 * Private global variables and functions
 **********************************************************************************************************************/
void comp_lp_int_isr(void);

/* Version data structure used by error logger macro. */
static const fsp_version_t g_acmplp_version =
{
    .api_version_minor  = COMPARATOR_API_VERSION_MINOR,
    .api_version_major  = COMPARATOR_API_VERSION_MAJOR,
    .code_version_major = ACMPLP_CODE_VERSION_MAJOR,
    .code_version_minor = ACMPLP_CODE_VERSION_MINOR
};

/* Maps valid enums to register values. */
static const uint8_t acmplp_filter_map[] =
{
    [COMPARATOR_FILTER_OFF] = 0U,
    [COMPARATOR_FILTER_1]   = 1U,
    [COMPARATOR_FILTER_8]   = 2U,
    [COMPARATOR_FILTER_32]  = 3U,
};

/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/

/* ACMPLP Implementation of comparator interface. */
const comparator_api_t g_comparator_on_acmplp =
{
    .open         = R_ACMPLP_Open,
    .outputEnable = R_ACMPLP_OutputEnable,
    .infoGet      = R_ACMPLP_InfoGet,
    .statusGet    = R_ACMPLP_StatusGet,
    .close        = R_ACMPLP_Close,
    .versionGet   = R_ACMPLP_VersionGet
};

/*******************************************************************************************************************//**
 * @addtogroup ACMPLP
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Configures the comparator and starts operation. Callbacks and pin output are not active until outputEnable() is
 * called. @ref comparator_api_t::outputEnable() should be called after the output has stabilized. Implements
 * @ref comparator_api_t::open().
 *
 * Comparator inputs must be configured in the application code prior to calling this function.
 *
 * @retval FSP_SUCCESS                    Open successful.
 * @retval FSP_ERR_ASSERTION              An input pointer is NULL
 * @retval FSP_ERR_INVALID_ARGUMENT       An argument is invalid. Window mode (COMPARATOR_MODE_WINDOW) and filter of 1
 *                                        (COMPARATOR_FILTER_1) are not supported in this implementation.
 *                                        p_cfg->p_callback is not NULL, but ISR is not enabled. ISR must be enabled to
 *                                        use callback function.
 * @retval FSP_ERR_ALREADY_OPEN           The control block is already open or the hardware lock is taken.
 **********************************************************************************************************************/
fsp_err_t R_ACMPLP_Open (comparator_ctrl_t * const p_ctrl, comparator_cfg_t const * const p_cfg)
{
    acmplp_instance_ctrl_t * p_instance_ctrl = (acmplp_instance_ctrl_t *) p_ctrl;

#if ACMPLP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_cfg);
    FSP_ASSERT(NULL != p_instance_ctrl);

    /* COMPARATOR_FILTER_16 is not supported for this implementation. */
    FSP_ERROR_RETURN(COMPARATOR_FILTER_16 != p_cfg->filter, FSP_ERR_INVALID_ARGUMENT);

    /* Callback should not be NULL when interrupt is configured */
    if (p_cfg->irq >= 0)
    {
        FSP_ERROR_RETURN(BSP_IRQ_DISABLED != p_cfg->ipl, FSP_ERR_INVALID_ARGUMENT);
        FSP_ASSERT(NULL != p_cfg->p_callback);
    }

    /* Verify the control block has not already been initialized. */
    FSP_ERROR_RETURN(ACMPLP_OPEN != p_instance_ctrl->open, FSP_ERR_ALREADY_OPEN);
#endif

    /* Save the configuration  */
    p_instance_ctrl->p_cfg = p_cfg;

    /* Disable Output at initialization */
    p_instance_ctrl->output_enabled = 0U;

    /* Configure interrupt priority. The interrupt is disabled until @ref comparator_api_t::outputEnable() is called. */
    if (p_instance_ctrl->p_cfg->irq >= 0)
    {
        R_BSP_IrqCfg(p_instance_ctrl->p_cfg->irq, p_cfg->ipl, p_instance_ctrl);
    }

    /* Power on ACMPLP before setting any hardware registers.*/
    R_BSP_MODULE_START(FSP_IP_ACMPLP, p_cfg->channel);

    /* ACMPLP register settings described in Table 39.3 "Procedure for setting the ACMPLP associated registers
     * (i = 0, 1)" of the RA4M1 manual R01UH0887EJ0100 are done here with the exception of enabling interrupts and VCOUT.
     * Interrupts and VCOUT are enabled as configured in R_ACMPLP_OutputEnable() because they should only be enabled
     * after the stabilization wait time has elapsed.*/
    acmplp_hardware_initialize(p_instance_ctrl);

    /* Mark the control block as open */
    p_instance_ctrl->open = ACMPLP_OPEN;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Provides the minimum stabilization wait time in microseconds. Implements @ref comparator_api_t::infoGet().
 *
 * @retval  FSP_SUCCESS                Information stored in p_info.
 * @retval  FSP_ERR_ASSERTION          An input pointer was NULL.
 * @retval  FSP_ERR_NOT_OPEN           Instance control block is not open.
 **********************************************************************************************************************/
fsp_err_t R_ACMPLP_InfoGet (comparator_ctrl_t * const p_ctrl, comparator_info_t * const p_info)
{
    acmplp_instance_ctrl_t * p_instance_ctrl = (acmplp_instance_ctrl_t *) p_ctrl;

    /* Perform parameter checking  */
#if (1 == ACMPLP_CFG_PARAM_CHECKING_ENABLE)

    /* Verify the pointers are not NULL and ensure the ACMPLP is already open. */
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ASSERT(NULL != p_info);
    FSP_ERROR_RETURN(ACMPLP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#else
    FSP_PARAMETER_NOT_USED(p_instance_ctrl);
#endif

    /* This comes from the Operation section in the hardware manual. */
    p_info->min_stabilization_wait_us = BSP_FEATURE_ACMPLP_MIN_WAIT_TIME_US;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Enables the comparator output, which can be polled using @ref comparator_api_t::statusGet(). Also enables pin output and
 * interrupts as configured during @ref comparator_api_t::open(). Implements @ref comparator_api_t::outputEnable().
 *
 * @retval  FSP_SUCCESS                Comparator output is enabled.
 * @retval  FSP_ERR_ASSERTION          An input pointer was NULL.
 * @retval  FSP_ERR_NOT_OPEN           Instance control block is not open.
 **********************************************************************************************************************/
fsp_err_t R_ACMPLP_OutputEnable (comparator_ctrl_t * const p_ctrl)
{
    acmplp_instance_ctrl_t * p_instance_ctrl = (acmplp_instance_ctrl_t *) p_ctrl;

#if (1 == ACMPLP_CFG_PARAM_CHECKING_ENABLE)

    /* Verify the pointers are not NULL and ensure the ACMPLP is already open. */
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(ACMPLP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    /* NOTE: All write instructions to registers with bits for more than one channel must be
     * in critical sections to ensure the driver is reentrant for different channels. */
    FSP_CRITICAL_SECTION_DEFINE;
    FSP_CRITICAL_SECTION_ENTER;

    /* Set the VCOUT output setting for this channel (enabled or disabled). */
    if (0U == p_instance_ctrl->p_cfg->channel)
    {
        R_ACMPLP->COMPOCR_b.C0OE = (uint8_t) p_instance_ctrl->p_cfg->pin_output & 1U;
    }
    else
    {
        R_ACMPLP->COMPOCR_b.C1OE = (uint8_t) p_instance_ctrl->p_cfg->pin_output & 1U;
    }

    FSP_CRITICAL_SECTION_EXIT;

    /* Enable interrupts for this channel. */
    if (p_instance_ctrl->p_cfg->irq >= 0)
    {
        R_BSP_IrqEnable(p_instance_ctrl->p_cfg->irq);
    }

    p_instance_ctrl->output_enabled = 1U;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Provides the operating status of the comparator. Implements @ref comparator_api_t::statusGet().
 *
 * @retval  FSP_SUCCESS                Operating status of the comparator is provided in p_status.
 * @retval  FSP_ERR_ASSERTION          An input pointer was NULL.
 * @retval  FSP_ERR_NOT_OPEN           Instance control block is not open.
 **********************************************************************************************************************/
fsp_err_t R_ACMPLP_StatusGet (comparator_ctrl_t * const p_ctrl, comparator_status_t * const p_status)
{
    acmplp_instance_ctrl_t * p_instance_ctrl = (acmplp_instance_ctrl_t *) p_ctrl;

#if (1 == ACMPLP_CFG_PARAM_CHECKING_ENABLE)

    /* Verify the pointers are not NULL and ensure the ACMPLP is already open. */
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ASSERT(NULL != p_status);
    FSP_ERROR_RETURN(ACMPLP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    uint8_t state = 0;

    if (0U == p_instance_ctrl->output_enabled)
    {
        p_status->state = COMPARATOR_STATE_OUTPUT_DISABLED;
    }
    else
    {
        /* Read the operating status of the comparator. */
        if (0U == p_instance_ctrl->p_cfg->channel)
        {
            state = R_ACMPLP->COMPMDR_b.C0MON;
        }
        else
        {
            state = R_ACMPLP->COMPMDR_b.C1MON;
        }

        /* Check for Output Polarity Selection */
        if (COMPARATOR_POLARITY_INVERT_ON == p_instance_ctrl->p_cfg->invert)
        {
            state = !state;
        }

        p_status->state = (comparator_state_t) state;
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Stops the comparator. Implements @ref comparator_api_t::close().
 *
 * @retval  FSP_SUCCESS                Instance control block closed successfully.
 * @retval  FSP_ERR_ASSERTION          An input pointer was NULL.
 * @retval  FSP_ERR_NOT_OPEN           Instance control block is not open.
 **********************************************************************************************************************/
fsp_err_t R_ACMPLP_Close (comparator_ctrl_t * p_ctrl)
{
    acmplp_instance_ctrl_t * p_instance_ctrl = (acmplp_instance_ctrl_t *) p_ctrl;

    /* Perform parameter checking*/
#if (1 == ACMPLP_CFG_PARAM_CHECKING_ENABLE)

    /* Verify the pointers are not NULL and ensure the ACMPLP is already open. */
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(ACMPLP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    /* Mark driver as closed   */
    p_instance_ctrl->open = 0U;

    /* Disable interrupt. */
    if (p_instance_ctrl->p_cfg->irq >= 0)
    {
        R_BSP_IrqDisable(p_instance_ctrl->p_cfg->irq);
        R_FSP_IsrContextSet(p_instance_ctrl->p_cfg->irq, NULL);
    }

    /* Stop the comparator and disable output to VCOUT. */
    FSP_CRITICAL_SECTION_DEFINE;
    FSP_CRITICAL_SECTION_ENTER;

    if (0U == p_instance_ctrl->p_cfg->channel)
    {
        /* Clear the Operation enable and output enable */
        R_ACMPLP->COMPMDR_b.C0ENB = 0U;
        R_ACMPLP->COMPOCR_b.C0OE  = 0U;
    }
    else
    {
        R_ACMPLP->COMPMDR_b.C1ENB = 0U;
        R_ACMPLP->COMPOCR_b.C1OE  = 0U;
    }

    FSP_CRITICAL_SECTION_EXIT;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Gets the API and code version. Implements @ref comparator_api_t::versionGet().
 *
 * @retval  FSP_SUCCESS        Version information available in p_version.
 * @retval  FSP_ERR_ASSERTION  The parameter p_version is NULL.
 **********************************************************************************************************************/
fsp_err_t R_ACMPLP_VersionGet (fsp_version_t * const p_version)
{
#if (1 == ACMPLP_CFG_PARAM_CHECKING_ENABLE)

    /* Verify the pointer is not NULL. */
    FSP_ASSERT(NULL != p_version);
#endif

    /* Return the version number */
    p_version->version_id = g_acmplp_version.version_id;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @} (end addtogroup ACMPLP)
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Performs hardware initialization of the ACMPLP.
 *
 * @param[in]  p_instance_ctrl        Instance control block.
 **********************************************************************************************************************/
static void acmplp_hardware_initialize (acmplp_instance_ctrl_t * const p_instance_ctrl)
{
    uint8_t temp_reg = 0;

    /* Configure the trigger edge and the hardware debounce filter
     * Set CiEPO and CiEDG at the same time since they are controlled by the same enum.
     * The register value to write is one less than the value in the interface. */
    uint8_t compfir = (uint8_t) ((p_instance_ctrl->p_cfg->trigger - 1) << 2);
    compfir |= acmplp_filter_map[p_instance_ctrl->p_cfg->filter];

    /* Configure Window function mode */
    uint8_t compmdr = (uint8_t) ((p_instance_ctrl->p_cfg->mode) << 1);

    /* Configure Reference Voltage Selection */
    compmdr |= (uint8_t) ((p_instance_ctrl->p_cfg->vref_select) << R_ACMPLP_COMPMDR_C0VRF_Pos);

    /* Configure the output polarity. */
    uint8_t compocr = (uint8_t) p_instance_ctrl->p_cfg->invert;

#if BSP_FEATURE_ACMPLP_HAS_COMPSEL_REGISTERS
    r_acmplp_extended_cfg_t * p_extend = ((r_acmplp_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend);
    uint8_t compsel1 = 0;
#endif

    /* NOTE: All read-modify-write instructions to registers with bits for more than one channel must be
     * in critical sections to ensure the driver is reentrant for different channels. */
    FSP_CRITICAL_SECTION_DEFINE;
    FSP_CRITICAL_SECTION_ENTER;

    if (0U == p_instance_ctrl->p_cfg->channel)
    {
#if BSP_FEATURE_ACMPLP_HAS_COMPSEL_REGISTERS

        /* Configure the input voltage to the comparator */
        R_ACMPLP->COMPSEL0 &= ACMPLP_COMPSEL_CH0_CLEAR;
        R_ACMPLP->COMPSEL0 |= (uint8_t) p_extend->input_voltage;

        /* Clear the COMPSEL1 to update the CiWDE bit
         * See note 5 & 6 in section "39.2.1 ACMPLP Mode Setting Register (COMPMDR)" of the RA4M1 manual R01UH0887EJ0100*/
        compsel1           = R_ACMPLP->COMPSEL1;
        compsel1          &= ACMPLP_COMPSEL_CH0_CLEAR;
        R_ACMPLP->COMPSEL1 = compsel1;
#endif

        /* Configure the filter select (CnFCK), edge polarity (CnEPO) and egde detection (CnEDG) */
        temp_reg          = R_ACMPLP->COMPFIR;
        temp_reg         &= ACMPLP_COMPFIR_CH0_CLEAR;
        temp_reg         |= compfir;
        R_ACMPLP->COMPFIR = temp_reg;

        /* Configure the Output polarity and set comparator to high-speed mode*/
        temp_reg          = R_ACMPLP->COMPOCR;
        temp_reg         &= ACMPLP_COMPOCR_CH0_CLEAR;
        temp_reg         |= (uint8_t) (compocr << R_ACMPLP_COMPOCR_C0OP_Pos);
        R_ACMPLP->COMPOCR = temp_reg | ACMPLP_CPMPOCR_SPDMD_SET;

        /* Configure Window mode and enable comparator */
        temp_reg          = R_ACMPLP->COMPMDR;
        temp_reg         &= ACMPLP_COMPMDR_CH0_CLEAR;
        temp_reg         |= compmdr;
        R_ACMPLP->COMPMDR = temp_reg;

#if BSP_FEATURE_ACMPLP_HAS_COMPSEL_REGISTERS
        compsel1          |= (p_extend->reference_voltage);
        R_ACMPLP->COMPSEL1 = compsel1;
#endif

        /* Enable the comparator. */
        R_ACMPLP->COMPMDR |= 1;
    }
    else
    {
#if BSP_FEATURE_ACMPLP_HAS_COMPSEL_REGISTERS

        /* Configure the input voltage to the comparator */
        R_ACMPLP->COMPSEL0 &= ACMPLP_COMPSEL_CH1_CLEAR;
        R_ACMPLP->COMPSEL0 |= (uint8_t) (p_extend->input_voltage << R_ACMPLP_COMPSEL0_IVCMP1_Pos);

        compsel1  = R_ACMPLP->COMPSEL1;
        compsel1 &= ACMPLP_COMPSEL_CH1_CLEAR;

        if ((ACMPLP_CFG_CH1_IVREF0_ENABLE == R_ACMPLP->COMPSEL1_b.C1VRF2) &&
            (p_instance_ctrl->p_cfg->mode == R_ACMPLP->COMPMDR_b.C1WDE) &&
            (p_instance_ctrl->p_cfg->vref_select == R_ACMPLP->COMPMDR_b.C1VRF))
        {
            /* If C1VRF2 and C1WDE already has desired value then do not clear COMPSEL1 */
            R_ACMPLP->COMPSEL1 = compsel1;
        }
        else
        {
            /* To change C1WDE and C1VRF, the CRV[6:4] and CRV[2:0] bits must be 000b.
             * See note 5 & 6 in section "39.2.1 ACMPLP Mode Setting Register (COMPMDR)" of the RA4M1 manual R01UH0887EJ0100*/

            /* To change C1VRF2, bits CRVS[6:4] and CRVS[2:0] must be 000b
             * See note 3 in section 39.2.5 Comparator Reference Voltage Select Register (COMPSEL1) of the RA4M1 manual R01UH0887EJ0100*/
            R_ACMPLP->COMPSEL1 = 0;
        }
#endif

        /* Configure the filter select (CnFCK), edge polarity (CnEPO) and egde detection (CnEDG) */
        temp_reg          = R_ACMPLP->COMPFIR;
        temp_reg         &= ACMPLP_COMPFIR_CH1_CLEAR;
        temp_reg         |= (uint8_t) (compfir << R_ACMPLP_COMPFIR_C1FCK_Pos);
        R_ACMPLP->COMPFIR = temp_reg;

        /* Configure the Output polarity and set comparator to high-speed mode*/
        temp_reg          = R_ACMPLP->COMPOCR;
        temp_reg         &= ACMPLP_COMPOCR_CH1_CLEAR;
        temp_reg         |= (uint8_t) (compocr << R_ACMPLP_COMPOCR_C1OP_Pos);
        R_ACMPLP->COMPOCR = temp_reg | ACMPLP_CPMPOCR_SPDMD_SET;

        /* Configure Window mode and enable comparator */
        temp_reg          = R_ACMPLP->COMPMDR;
        temp_reg         &= ACMPLP_COMPMDR_CH1_CLEAR;
        temp_reg         |= (uint8_t) (compmdr << R_ACMPLP_COMPMDR_C1ENB_Pos);
        R_ACMPLP->COMPMDR = temp_reg;

#if BSP_FEATURE_ACMPLP_HAS_COMPSEL_REGISTERS
 #if (ACMPLP_CFG_CH1_IVREF0_ENABLE)
        temp_reg = (uint8_t) (p_extend->reference_voltage); // IVREF0
 #else

        /* Configure the reference voltage to the comparator and Reference Voltage Selection 2 for channel 1 */
        temp_reg  = (uint8_t) 1 << R_ACMPLP_COMPSEL1_C1VRF2_Pos;
        temp_reg |= (uint8_t) (p_extend->reference_voltage << R_ACMPLP_COMPSEL1_IVREF1_Pos); // IVREF1
 #endif

        /* Retain CRVS for channel 0 and update CRVS for channel 1 */
        compsel1          |= temp_reg;
        R_ACMPLP->COMPSEL1 = compsel1;
#endif

        /* Enable the comparator. */
        R_ACMPLP->COMPMDR |= (1 << R_ACMPLP_COMPMDR_C1ENB_Pos);
    }

    FSP_CRITICAL_SECTION_EXIT;
}

/*******************************************************************************************************************//**
 * Comparator interrupt
 **********************************************************************************************************************/
void comp_lp_int_isr (void)
{
    /* Save context if RTOS is used */
    FSP_CONTEXT_SAVE

    IRQn_Type                irq             = R_FSP_CurrentIrqGet();
    acmplp_instance_ctrl_t * p_instance_ctrl = (acmplp_instance_ctrl_t *) R_FSP_IsrContextGet(irq);

    /* Clear the IR flag in the ICU */
    R_BSP_IrqStatusClear(irq);

    /* Call user callback if one was provided. */
    comparator_callback_args_t args;
    args.channel   = p_instance_ctrl->p_cfg->channel;
    args.p_context = p_instance_ctrl->p_cfg->p_context;
    p_instance_ctrl->p_cfg->p_callback(&args);

    /* Restore context if RTOS is used */
    FSP_CONTEXT_RESTORE
}
