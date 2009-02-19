
KDIR = /proj/aufs/git

install_sbin: | pre_install_sbin
install_ubin: | pre_install_ubin
install_etc: | pre_install_etc
pre_install_sbin pre_install_ubin pre_install_etc:
	sudo ln -sf $(addprefix ${CURDIR}/, ${File}) ${Tgt}

########################################

ifeq (${HOSTNAME},jrobl)

CFLAGS += -g -O0 -DDEBUG
lf := ${LDFLAGS}
LDFLAGS = $(filer-out -static -s,${lf})
#${Bin}: LDLIBS =
#${Bin}: %: %.o ${LibObj}

%.i: %.c
	$(CC) -c -E $(CPPFLAGS) $(CFLAGS) $< | uniq

_Ver = $(shell fgrep 'define AUFS_VERSION' ${KDIR}/include/linux/aufs_type.h |\
	awk '{print $$3}')
Ver = $(shell echo ${_Ver})
Tar = ${Ver}-util.tar
tar: ${Tar}.bz2
${Tar}.bz2: ${Tar}
	cp -ip $< $<.tmp
	bzip2 $<
	mv $<.tmp $<
${Tar}: Makefile *.[ch] ${Cmd} aufs.shlib
	tar -cf $@ $^

clean: | pre_clean
pre_clean:
	${RM} ${Tar} ${Tar}.bz2

########################################

_br _br0: d1=/tmp/br0/.wh..wh.plnk
_br _br0: d2=/tmp/aufs/si_1
_br0:
	mkdir -p ${d1} ${d2}
	touch /tmp/a /tmp/b
	echo /tmp/br0=rw > ${d2}/br0
	echo /tmp/br1=ro > ${d2}/br1
	cp -p /etc/mtab /tmp
_br: _br0
	ln -f /tmp/a ${d1}/$(shell stat -c %i /tmp/a).00
	ln -f /tmp/b ${d1}/$(shell stat -c %i /tmp/b).00

debug: _br
	MALLOC_CHECK_=2 gdb --args auplink /tmp list
#	MALLOC_CHECK_=2 gdb --args mount.aufs none /tmp -v -o remount,fuck
#	MALLOC_CHECK_=2 gdb --args mount.aufs /usr/bin /tmp -v -o bind
#	MALLOC_CHECK_=2 gdb --args mount.aufs none /tmp -v -o remount,noplink
#	MALLOC_CHECK_=2 gdb --args mount.aufs none /tmp -v -o br:/tmp/br0:/tmp/br1
endif
