#
# $Id: Makefile,v 1.2 2002/12/31 23:43:34 te Exp $
#

OBJS		= m_pool.o mem_watch.o
TARGET		= mw
INSTALLDIR	= /home/te/bin
TARBALL		= memwatch.tar.gz
PROGDIR		= memwatch

# External files (get via: make import).
EXT_ROOT	= $(C_ROOT)/comm
EXT_DIRS	= win32
EXT_FILES	= my_bitstring.h m_pool.h m_pool.c win32/bsd_list.h

.include <unix.prog.c.mk>
