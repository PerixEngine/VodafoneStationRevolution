SMBDIR=..
include $(SMBDIR)/envir.mak

JMK_A_TARGET=librpc_client.a
JMK_O_OBJS=../rpcclient/rpcclient.o ../rpcclient/cmd_lsarpc.o \
  ../rpcclient/cmd_samr.o ../rpcclient/cmd_spoolss.o \
  ../rpcclient/cmd_netlogon.o ../rpcclient/cmd_srvsvc.o \
  ../rpcclient/cmd_dfs.o \
  ../rpcclient/cmd_ds.o ../rpcclient/cmd_echo.o ../rpcclient/cmd_shutdown.o \
  cli_pipe.o cli_lsarpc.o cli_samr.o cli_netlogon.o cli_srvsvc.o \
  cli_dfs.o cli_reg.o cli_pipe.o cli_spoolss.o \
  cli_spoolss_notify.o cli_ds.o cli_echo.o cli_shutdown.o

include $(SMBMK)
