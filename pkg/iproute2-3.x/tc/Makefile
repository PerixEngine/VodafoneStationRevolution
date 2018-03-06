ifndef JMKE_ENV_INCLUDED
  JMK_ROOT=../../../
  include $(JMK_ROOT)/jmk/env_root.mak
endif

JMK_WARN2ERR=n

YCCFLAGS+=-d

JMK_CFLAGS+=-DCONFIG_GACT -DCONFIG_GACT_PROB -DYY_NO_INPUT
JMK_LDFLAGS+=-lm -ldl -Wl,-export-dynamic

JMK_TARGET=tc
JMK_LIBS_tc=$(LIBNETLINK) $(JMKE_PWD_BUILD)/libtc.a \
  $(JMKE_BUILDDIR)/pkg/build/lib/libm.a

JMK_O_OBJS_tc=tc.o tc_qdisc.o tc_class.o tc_filter.o tc_util.o tc_monitor.o \
  m_police.o m_estimator.o m_action.o m_ematch.o emp_ematch.yacc.o \
  emp_ematch.lex.o q_fifo.o q_sfq.o q_red.o q_prio.o q_tbf.o q_cbq.o q_rr.o \
  q_multiq.o q_netem.o q_choke.o q_sfb.o f_rsvp.o f_u32.o f_route.o f_fw.o \
  f_basic.o f_flow.o f_cgroup.o q_dsmark.o q_gred.o f_tcindex.o q_ingress.o \
  q_hfsc.o q_htb.o q_drr.o q_qfq.o m_gact.o m_mirred.o m_nat.o m_pedit.o \
  m_skbedit.o m_csum.o p_ip.o p_icmp.o p_tcp.o p_udp.o em_nbyte.o em_cmp.o \
  em_u32.o em_meta.o q_mqprio.o

JMK_RAMDISK_BIN_FILES+=$(JMK_TARGET)

JMK_INTERNAL_HEADERS+=tc_core.h tc_common.h tc_cbq.h tc_red.h tc_util.h \
  m_ematch.h m_pedit.h

JMK_A_TARGET=libtc.a
JMK_O_OBJS_libtc.a=tc_core.o tc_red.o tc_cbq.o tc_estimator.o tc_stab.o

JMK_AUTOGEN_SRC=emp_ematch.yacc.c emp_ematch.yacc.h emp_ematch.lex.c

%.yacc.c: %.y
	$(YACC) $(YCCFLAGS) -o $@ $<

%.lex.c: %.l
	$(LEX) $(LEXFLAGS) -o $@ $<

JMK_CLEAN+=$(JMK_AUTOGEN_SRC)

$(call JMKE_INCLUDE_RULES)
