CC = gcc
CFLAGS = -Wall -Wextra -Icommon -Ihub -pthread -g
HUB_LDFLAGS = -lpthread -lsqlite3 -lrt
SENSOR_LDFLAGS = -lpthread -lrt

COMMON_SRC = common/ipc.c
COMMON_OBJ = common/ipc.o

SENSOR_SRC = sensors/sensor_common.c

HUB_SRC = hub/hub.c hub/ringbuf.c hub/db.c hub/alarm.c hub/web.c
HUB_OBJ = hub/hub.o hub/ringbuf.o hub/db.o hub/alarm.o hub/web.o

.PHONY: all clean

all: smarthome_hub temp_sensor humidity_sensor light_sensor smoke_sensor

common/ipc.o: common/ipc.c common/ipc.h common/common.h
	$(CC) $(CFLAGS) -c -o $@ $<

hub/ringbuf.o: hub/ringbuf.c hub/ringbuf.h common/common.h
	$(CC) $(CFLAGS) -c -o $@ $<

hub/db.o: hub/db.c hub/db.h common/common.h
	$(CC) $(CFLAGS) -c -o $@ $<

hub/alarm.o: hub/alarm.c hub/alarm.h common/common.h
	$(CC) $(CFLAGS) -c -o $@ $<

hub/web.o: hub/web.c hub/web.h common/common.h
	$(CC) $(CFLAGS) -c -o $@ $<

hub/hub.o: hub/hub.c hub/ringbuf.h hub/db.h hub/alarm.h hub/web.h common/ipc.h common/common.h
	$(CC) $(CFLAGS) -c -o $@ $<

smarthome_hub: $(COMMON_OBJ) $(HUB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(HUB_LDFLAGS)

sensors/sensor_common.o: sensors/sensor_common.c sensors/sensor_common.h common/ipc.h common/common.h
	$(CC) $(CFLAGS) -c -o $@ $<

sensors/temp_sensor.o: sensors/temp_sensor.c sensors/sensor_common.h common/ipc.h common/common.h
	$(CC) $(CFLAGS) -c -o $@ $<

temp_sensor: common/ipc.o sensors/sensor_common.o sensors/temp_sensor.o
	$(CC) $(CFLAGS) -o $@ $^ $(SENSOR_LDFLAGS)

humidity_sensor: common/ipc.o sensors/sensor_common.o sensors/humidity_sensor.o
	$(CC) $(CFLAGS) -o $@ $^ $(SENSOR_LDFLAGS)

light_sensor: common/ipc.o sensors/sensor_common.o sensors/light_sensor.o
	$(CC) $(CFLAGS) -o $@ $^ $(SENSOR_LDFLAGS)

smoke_sensor: common/ipc.o sensors/sensor_common.o sensors/smoke_sensor.o
	$(CC) $(CFLAGS) -o $@ $^ $(SENSOR_LDFLAGS)

sensors/%.o: sensors/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f smarthome_hub temp_sensor humidity_sensor light_sensor smoke_sensor
	rm -f common/*.o hub/*.o sensors/*.o
	rm -f smarthome.db
	rm -f /tmp/smarthome_hub.sock
