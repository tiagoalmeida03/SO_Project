#\
	Gonçalo Monteiro 	2021217127\
	Tiago Almeida		2021221615

CC				= gcc
FLAGS			= -Wall -pthread -D_REENTRANT

SOURCE_DIR		= source
OBJS_DIR		= objs

SYS_MANAGER		= sys_manager
SENSOR			= sensor
CONSOLE			= console

SYS_MANAGER_OBJ	= ${addprefix ${OBJS_DIR}/, sys_manager.o sys_manager_globals.o project_consts.o worker.o alerts_watcher.o log.o semaphores.o queue.o}
SENSOR_OBJ		= ${addprefix ${OBJS_DIR}/, sensor.o project_consts.o}
CONSOLE_OBJ		= ${addprefix ${OBJS_DIR}/, console.o project_consts.o}


all:		${SYS_MANAGER} ${SENSOR} ${CONSOLE}

clean:
			rm -r ${SYS_MANAGER} ${SENSOR} ${CONSOLE} ${OBJS_DIR}

${SYS_MANAGER}:${SYS_MANAGER_OBJ}
			${CC} ${FLAGS} ${SYS_MANAGER_OBJ} -o $@

${SENSOR}:	${SENSOR_OBJ}
			${CC} ${FLAGS} ${SENSOR_OBJ} -o $@

${CONSOLE}:	${CONSOLE_OBJ}
			${CC} ${FLAGS} ${CONSOLE_OBJ} -o $@

${OBJS_DIR}/%.o: ${SOURCE_DIR}/%.c
			mkdir -p ${OBJS_DIR}
			${CC} ${FLAGS} $< -c -o $@
