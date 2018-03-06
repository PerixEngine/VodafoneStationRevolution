ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

ifeq ($(CONFIG_MIPS), y)
bits=
ifdef CONFIG_CPU_MIPS32
  bits=32
endif
ifdef CONFIG_CPU_MIPS64
  bits=64
endif

JMK_AUTOGEN_SRC+=mips.S
JMK_CLEAN+=mips.S

JMK_CD_EXPORTED_FILES+=mips.pl

$(JMKE_PWD_BUILD)/mips.S: $(JMKE_PWD_SRC)/mips.pl
	$(PERL) $(JMKE_DEPS) $(bits) > $@
endif

$(call JMKE_INCLUDE_RULES)
