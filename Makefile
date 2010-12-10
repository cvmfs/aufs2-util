
# Copyright (C) 2005-2010 Junjiro R. Okajima
#
# This program, aufs is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301	 USA

CFLAGS += -I./libau
CFLAGS += -O -Wall

Cmd = umount.aufs auchk aubrsync
Man = aufs.5
Etc = etc_default_aufs
Bin = auplink mount.aufs #auctl
BinObj = $(addsuffix .o, ${Bin})
LibUtil = libautil.a
LibUtilObj = proc_mnt.o br.o plink.o mtab.o
LibUtilHdr = au_util.h
export

all: ${Man} ${Bin} ${Etc}
	${MAKE} -C libau $@
	ln -sf ./libau/libau*.so .

${Bin}: LDFLAGS += -static -s
${Bin}: LDLIBS = -L. -lautil
${BinObj}: %.o: %.c ${LibUtilHdr} ${LibUtil}

${LibUtilObj}: %.o: %.c ${LibUtilHdr}
#${LibUtil}: ${LibUtil}(${LibUtilObj})
${LibUtil}: $(foreach o, ${LibUtilObj}, ${LibUtil}(${o}))
.NOTPARALLEL: ${LibUtil}

etc_default_aufs: c2sh aufs.shlib
	${RM} $@
	echo '# aufs variables for shell scripts' > $@
	./c2sh >> $@
	echo >> $@
	sed -e '0,/^$$/d' aufs.shlib >> $@

aufs.5: aufs.in.5 c2tmac
	${RM} $@
	./c2tmac > $@
	awk '{ \
		gsub(/\140[^\047]*\047/, "\\[oq]&\\[cq]"); \
		gsub(/\\\[oq\]\140/, "\\[oq]"); \
		gsub(/\047\\\[cq\]/, "\\[cq]"); \
		gsub(/\047/, "\\[aq]"); \
		print; \
	}' aufs.in.5 >> $@
	chmod a-w $@

.INTERMEDIATE: c2sh c2tmac

Install = install -o root -g root -p
install_sbin: File = mount.aufs umount.aufs auplink
install_sbin: Tgt = ${DESTDIR}/sbin
install_ubin: File = auchk aubrsync #auctl
install_ubin: Tgt = ${DESTDIR}/usr/bin
install_sbin install_ubin: ${File}
	install -d ${Tgt}
	${Install} -m 755 ${File} ${Tgt}
install_etc: File = etc_default_aufs
install_etc: Tgt = ${DESTDIR}/etc/default/aufs
install_etc: ${File}
	install -d $(dir ${Tgt})
	${Install} -m 644 -T ${File} ${Tgt}
install_man: File = aufs.5
install_man: Tgt = ${DESTDIR}/usr/share/man/man5
install_man: ${File}
	install -d ${Tgt}
	${Install} -m 644 ${File} ${Tgt}
install_ulib:
	${MAKE} -C libau $@

install: install_man install_sbin install_ubin install_etc install_ulib

clean:
	${RM} ${Man} ${Bin} ${Etc} ${LibUtil} libau.so* *~
	${RM} ${BinObj} ${LibUtilObj}
	${MAKE} -C libau $@

-include priv.mk
