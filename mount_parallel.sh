fusermount -u home
sshfs -p 60001 s49517@remote.vdi.mipt.ru:. home -oauto_cache,reconnect,transform_symlinks,follow_symlinks
