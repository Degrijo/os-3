#include "Queue.h"
#include <Windows.h>


Queue::Queue(int maxElements)
	:mutex(mutex_semaphore[0]), semaphore(mutex_semaphore[1])
{
	arrElements = (pElement)
		HeapAlloc(GetProcessHeap(), 0, sizeof(Element) * maxElements);
	this->maxElements = maxElements; mutex = CreateMutex(NULL, FALSE, NULL);
	semaphore = CreateSemaphore(NULL, 0, maxElements, NULL);

}

Queue::~Queue()
{
	CloseHandle(semaphore);
	CloseHandle(mutex);
	HeapFree(GetProcessHeap(), 0, arrElements);

}

BOOL Queue::put(pElement element, DWORD milliseseconds)
{
	BOOL fOk = FALSE; DWORD dw = WaitForSingleObject(mutex, milliseseconds);

	if (dw == WAIT_OBJECT_0) { // этот поток получил монопольный доступ к очереди
	// увеличиваем число элементов в очереди
		LONG lPrevCount; fOk = ReleaseSemaphore(semaphore, 1, &lPrevCount);
		if (fOk) {
			// в очереди еще есть место; добавляем новый элемент
			arrElements[lPrevCount] = *element; } else {
// очередь полностью заполнена; устанавливаем код ошибки 
// и сообщаем о неудачном завершении вызова
			SetLastError(ERROR_DATABASE_FULL);
		}
		// разрешаем другим потокам обращаться к очереди
		ReleaseMutex(mutex);
	}
	else { // время ожидания истекло; устанавливаем код ошибки 
		   // и сообщаем о неудачном завершении вызова
		SetLastError(ERROR_TIMEOUT); }
		return(fOk); // GetLastError сообщит дополнительную информацию

}

BOOL Queue::pull(pElement element, DWORD milliseconds)
{// ждем монопольного доступа к очереди
 // и появления в ней хотя бы одного элемента
	BOOL fOk = (WaitForMultipleObjects(sizeof(mutex_semaphore)/sizeof(HANDLE), mutex_semaphore, TRUE, milliseconds) == WAIT_OBJECT_0);
	if (fOk) { // в очереди есть элемент; извлекаем его 
		*element = arrElements[0];
	// сдвигаем остальные элементы вниз на одну позицию
		MoveMemory(&arrElements[0], &arrElements[1], sizeof(Element) * (maxElements - 1));
	// разрешаем другим потокам обращаться к очереди
		ReleaseMutex(mutex);
	}
	else { // время ожидания истекло; устанавливаем код ошибки
		   // и сообщаем о неудачном завершении вызова
		SetLastError(ERROR_TIMEOUT); }
		return(fOk); // GetLastError сообщит дополнительную информацию

}
