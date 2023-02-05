#include <device.h>
#include <zephyr.h>
void lock_approtect()
{
	if ((NRF_UICR->APPROTECT & UICR_APPROTECT_PALL_Msk) !=
	    (UICR_APPROTECT_PALL_Enabled << UICR_APPROTECT_PALL_Pos)) {
		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
		while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
		}

		NRF_UICR->APPROTECT =
			((NRF_UICR->APPROTECT & ~((uint32_t)UICR_APPROTECT_PALL_Msk)) |
			 (UICR_APPROTECT_PALL_Enabled << UICR_APPROTECT_PALL_Pos));

		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
		while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
		}
	}
}
