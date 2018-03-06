export DLM_TARGET_DIR=$(MAINFS_DIR)/dlmfs_dir
export DLM_STAGING_DIR=$(JMKE_BUILDDIR)/pkg/build/dlm_staging

# deb file for a DLM
# $1 - name of the DLM
JMKF_DLM_TARGET_DEB=$(DLM_TARGET_DIR)/$1.deb

# $1 - source file + path
# $2 - destination file path
# $3 - name of the strip function
DLM_CP_FUNC = \
  $(JMKE_MKDIR) $(dir $2) && \
  rm -f $2 && \
  $(JMKE_CP) -L $1 $2 && \
  $(call $3,$2)
