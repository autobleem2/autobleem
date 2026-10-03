cd /media/Autobleem/rc;grep -q "$(printf '\r')" boot.sh && sed -i 's/\r//g' boot.sh;. ./boot.sh
