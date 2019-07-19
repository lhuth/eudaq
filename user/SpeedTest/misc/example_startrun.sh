#!/usr/bin/env sh
BINPATH=../../../bin
#$BINPATH/euRun &
$BINPATH/euRun -n Ex0RunControl &
sleep 1
$BINPATH/euLog &
sleep 1
# The following data collectors are provided if you build user/eudet
#$BINPATH/euCliCollector -n DirectSaveDataCollector -t my_dc &
$BINPATH/euCliCollector -n EventIDSyncDataCollector -t my_dc &
#$BINPATH/euCliCollector -n TriggerIDSyncDataCollector -t my_dc &
$BINPATH/euCliProducer -n SpeedTestProducer -t my_pd0 &
