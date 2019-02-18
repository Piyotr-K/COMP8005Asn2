#!/bin/sh

HAMACHI_DST=/opt/logmein-hamachi

echo Stopping LogMeIn Hamachi engine ..
/etc/init.d/logmein-hamachi stop

echo Removing LogMeIn Hamachi service ..

if [ -x /usr/lib/lsb/remove_initd ] ; then
        /usr/lib/lsb/remove_initd /etc/init.d/logmein-hamachi
elif [ -d /etc/rc0.d ] ; then
        rm /etc/rc0.d/K01logmein-hamachi
        rm /etc/rc1.d/K01logmein-hamachi
        rm /etc/rc2.d/S01logmein-hamachi
        rm /etc/rc3.d/S01logmein-hamachi
        rm /etc/rc4.d/S01logmein-hamachi
        rm /etc/rc5.d/S01logmein-hamachi
        rm /etc/rc6.d/K01logmein-hamachi
elif [ -d /etc/init.d/rc0.d ] ; then
        rm /etc/init.d/rc0.d/K01logmein-hamachi
        rm /etc/init.d/rc1.d/K01logmein-hamachi
        rm /etc/init.d/rc2.d/S01logmein-hamachi
        rm /etc/init.d/rc3.d/S01logmein-hamachi
        rm /etc/init.d/rc4.d/S01logmein-hamachi
        rm /etc/init.d/rc5.d/S01logmein-hamachi
        rm /etc/init.d/rc6.d/K01logmein-hamachi
elif [ -d /etc/rc.d ] ; then
        rm /etc/rc.d/rc0.d/K01logmein-hamachi
        rm /etc/rc.d/rc1.d/K01logmein-hamachi
        rm /etc/rc.d/rc2.d/S01logmein-hamachi
        rm /etc/rc.d/rc3.d/S01logmein-hamachi
        rm /etc/rc.d/rc4.d/S01logmein-hamachi
        rm /etc/rc.d/rc5.d/S01logmein-hamachi
        rm /etc/rc.d/rc6.d/K01logmein-hamachi
else
        echo "Cannot install hamachi autostart scripts."
fi


rm -f /etc/init.d/logmein-hamachi
rm -rf /var/run/logmein-hamachi

echo Removing LogMeIn Hamachi symlink ..
rm /usr/bin/hamachi

echo Removing files from $HAMACHI_DST ..
rm -rf "$HAMACHI_DST"

echo LogMeIn Hamachi is removed.
