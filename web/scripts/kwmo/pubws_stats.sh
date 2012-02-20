#!/bin/sh

set -e -u

# Enable / Disable this script. Set to 1 to enable.
RUN=0

# FROM email
FROM="from_email"

# TO email
TO="to_email"


# Exit if script is disabled.
if [ "$RUN" != "1" ]; then exit; fi

# Now, in seconds
TNOW=`date +'%s'`

# Last midnight, in seconds
TMID=$(( $TNOW - ( $TNOW % 86400 ) ))
#echo $TMID

# Yesterday's midnight, in seconds
TYESMID=$(( $TMID - 86400 ))
#echo $TYESMID

# Yesterday's date (YYYY-MM-DD)
TYESDATE=`date -d "1 days ago" +"%Y-%m-%d"`

# Host
THOST=`cat /etc/mailname`

# Fetch stats
# (Temporary code for converting logs for later use...)
DAILY_USERS=`cat /var/log/apache2/kwmo-access.log.$TYESMID \
	| grep " /?kws_id=[^ \&]*&eid=[^ \&]*&sid=[^ \&]* " \
        | wc -l`

# Prepare mail
SUBJECT="Public Teamboxes statistics for $TYESDATE: $DAILY_USERS connections"
BODY="Public Teamboxes statistics:
Host: $THOST
Date: $TYESDATE
Connections: $DAILY_USERS"

# Send mail
echo "$BODY" | mail -a "From: $FROM" -s "$SUBJECT" $TO

