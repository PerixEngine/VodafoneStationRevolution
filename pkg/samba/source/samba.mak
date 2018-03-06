
ifdef JMK_TARGET
$(call RGDEP,$(JMK_TARGET),$(LIBS))	
endif

include $(JMKE_MK)
