mutex mtx;
condvar prod,cons;
int prod_count = 0, cons_count = 0;
struct kfifo cbuffer;

void fifoproc_open(bool abre_para_lectura) {
	lock(mtx);
	if (abre_para_lectura) {
		/* Consumidor */
		cons_count++;
		/* Despierta a un productor en la cola de espera */
		cond_signal(prod);
		/* Bloqueo hasta que el productor haya abierto su extremo de escritura */
		while(prod_count == 0)
			cond_wait(cons, mtx);
	}
	else {
		/* Productor */
		prod_count++;
		/* Despierta a un consumidor en la cola de espera */
		cond_signal(cons);
		/* Bloqueo hata que el consumidor haya abierto su extremo de lectura */
		while(cons_count == 0)
			cond_wait(prod, mtx);
	}
	unlock(mtx);
 }

int fifoproc_write(char* buff, int len) { 
	char kbuffer[MAX_KBUF];

	if (len > MAX_CBUFFER_LEN || len > MAX_KBUF) 
		return Error;
	
	if (copy_from_user(kbuffer, buff, len)) 
		return Error;

	lock(mtx);

	/* Esperar hasta que haya hueco para insertar (debe haber consumidores) */
	while (kfifo_avail(&cbuffer) < len && cons_count > 0)
		cond_wait(prod, mtx);

	/* Detectar fin de comunicación por error (consumidor cierra FIFO antes) */
	if (cons_count == 0) {
		unlock(mtx); 
		return -EPIPE;
	}

	kfifo_in(&cbuffer, kbuffer, len);

	/* Despertar a posible consumidor bloqueado */
	cond_signal(cons);

	unlock(mtx);

	return len; 
}

int fifoproc_read(const char* buff, int len) { 
	char kbuffer[MAX_KBUF];

	if (len > MAX_CBUFFER_LEN || len > MAX_KBUF) 
		return Error;

	lock(mtx);

	/* Esperar hasta que haya datos para leer (debe haber productores) */
	while (kfifo_len(&cbuffer) < len && prod_count > 0)
		cond_wait(cons, mtx);

	/* Detectar fin de comunicación por error (productor cierra FIFO antes) */
	if (kfifo_is_empty(&cbuffer) && prod_count == 0) {
		unlock(mtx); 
		return 0;
	}

	kfifo_out(&cbuffer, kbuffer, len);

	/* Despertar a posible productor bloqueado */
	cond_signal(prod);

	unlock(mtx);

	if(copy_to_user(buff, kbuffer, len))
		return Error;

	return len; 
}

void fifoproc_release(bool lectura) { 
	lock(mtx);
	if (lectura) {
		/* Consumidor */
		cons_count--;
		/* Despierta a un productor en la cola de espera del write */
		cond_signal(prod);
	}
	else {
		/* Productor */
		prod_count--;
		/* Despierta a un consumidor en la cola de espera del read */
		cond_signal(cons);
	}

	if (cons_count == 0 && prod_count == 0)
		kfifo_reset(&cbuffer);

	unlock(mtx);
}