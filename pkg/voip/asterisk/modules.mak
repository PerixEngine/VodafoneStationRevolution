$(foreach o,$(JMK_O_OBJS), \
    $(eval JMK_CFLAGS_$o:=-Dload_module=$(o:%.o=%)_load_module \
        -Dunload_module=$(o:%.o=%)_unload_module \
        -Dreload=$(o:%.o=%)_reload \
        -Dreload_if_changed=$(o:%.o=%)_reload_if_changed \
        -Ddescription=$(o:%.o=%)_description \
        -Dkey=$(o:%.o=%)_key \
        -Dusecount=$(o:%.o=%)_usecount))
