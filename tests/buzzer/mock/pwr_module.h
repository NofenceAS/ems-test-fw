
#ifndef _PWR_MODULE_H_
#define _PWR_MODULE_H_

enum pwr_requester_module {
	REQ_EP_MODULE = 0,
	REQ_SOUND_CONTROLLER = 1,
	REQ_END_OF_LIST
};

static inline int pwr_module_use_extclk(enum pwr_requester_module req, 
					 bool use_extclk)
{
	return 0;
}

#endif /* _PWR_MODULE_H_ */