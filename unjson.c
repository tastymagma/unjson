/*
 * unjson.c
 *
 * Small utility to extract JSON files on the shell
 *
 * 6/12/14 tastymagma <tastymagma@vmail.me>
 */


#define _POSIX_C_SOURCE 1 /* for fileno */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <json/json.h>

int IgnoreKeyError = 1;
int IgnoreNullValues = 1;


/* simple queue for json_object* */

#define QSEP (json_object *) 1 /* hack */

typedef struct list {
	json_object *item;
	struct list *next;
} list_t;

typedef struct queue {
	list_t *head;
	list_t *tail;
} queue_t;

void
enqueue(queue_t *q, json_object *item)
{
	if (q->tail != NULL) {
		q->tail->next = malloc(sizeof(list_t));
		q->tail = q->tail->next;
	} else {
		q->tail = malloc(sizeof(list_t));
		if (q->head == NULL)
			q->head = q->tail;
	}
	q->tail->item = item;
	q->tail->next = NULL;
}

json_object*
dequeue(queue_t *q)
{
	json_object *item;
	list_t *new_head;

	if (q->head == NULL) {
		return NULL;
	} else {
		if (q->head == q->tail)
			q->tail = NULL;
		item = q->head->item;
		new_head = q->head->next;
		free(q->head);
		q->head = new_head;

		return item;
	}
}


void
print_help()
{
	printf("Usage: unjson [-h] [-I] [-N] [key]/[index]/[-] "
			"[key]/[index]/[-] ...\n");
	printf(
			"  -h      Print this help and exit\n"
		 	"  -I      Continue after key/index errors\n"
			"  -N      Ignore null values\n"
		   "  [key]   Select only value of key of the object in current level\n"
			"  [index] Select only the index of array in current level\n"
			"  [-]     Go one level deeper\n"
			"            (if not preceded by key or index will select\n"
			"             all values of the current level)\n");
	printf("Reads a JSON file from stdin and prints a string representation\n"
		" of all selected elements. (if an element is complex (array or object)\n"
		" it will be printed as JSON)\n");
}


int
enqueue_all(queue_t *q, json_object *item)
{
	int i = 0;
	switch(json_object_get_type(item)) {
		case json_type_object:
			; // C quirk
			json_object_object_foreach(item, key, val) {
				enqueue(q, val);
				i++;
			}
			break;
		case json_type_array:
			for(i = 0; i < json_object_array_length(item); i++)
					enqueue(q, json_object_array_get_idx(item, i));
			break;
		default:
			enqueue(q, item);
			i++;
	}

	return i;
}

json_object*
get_element(json_object* jobj, char* key)
{
	json_object* elem;
	char* endptr;
	
	if (json_object_get_type(jobj) == json_type_object) {
		if (!(json_object_object_get_ex(jobj, key, &elem))
				&& IgnoreKeyError) {
			fprintf(stderr, "Couldn't find element with key \"%s\"\n", key);
			exit(1);
		}
	} else if (json_object_get_type(jobj) == json_type_array) {
		int idx = (int) strtol(key, &endptr, 10);
		if (endptr == key || *endptr != '\0') {
			fprintf(stderr, "Invalid index \"%s\"\n", key);
			exit(1);
		}
		if ((elem = json_object_array_get_idx(jobj, idx)) == NULL
				&& IgnoreKeyError) {
			fprintf(stderr, "Couldn't find element with index \"%d\"\n", idx);
			exit(1);
		}
	} else if (IgnoreKeyError) {
		fprintf(stderr, "Key/Index \"%s\" given but no object or array found.\n",
				key);
		exit(1);
	}

	return elem;
}

void
print_element(json_object* jobj)
{
	switch(json_object_get_type(jobj)) {
		case json_type_null:
		case json_type_boolean:
		case json_type_double:
		case json_type_int:
		case json_type_string:
			puts(json_object_get_string(jobj));
			break;
		case json_type_array:
		case json_type_object:
		default:
				puts(json_object_to_json_string(jobj));
		}
}


int
main(int argc, char *argv[])
{
	char** key = NULL;
	int key_len = 0;

	FILE *stream = stdin;
	char buf[256];
	size_t count;

	queue_t queue_def = {NULL, NULL};
	queue_t *queue = &queue_def;

	if (argc > 1) {
		if (strcmp("-h", argv[1]) == 0) {
			print_help();
			return 0;
		}
		if (strcmp("-I", argv[1]) == 0) {
			IgnoreKeyError = 0;
			argv++;
			argc--;
		}
		if (strcmp("-N", argv[1]) == 0) {
			IgnoreNullValues = 0;
			argv++;
			argc--;
		}
		key = argv+1;
		key_len = argc - 1;
	}


	/* Parse JSON from file */

	enum json_tokener_error jerr;
	json_object* jobj;
	json_tokener* tok = json_tokener_new();

	while((count = read(fileno(stream), buf, sizeof(buf))) > 0) {
		jobj = json_tokener_parse_ex(tok, buf, count);
		if ((jerr = json_tokener_get_error(tok)) != json_tokener_continue)
			break;
	}
	if (jerr != json_tokener_success || jobj == NULL) {
		fprintf(stderr, "Couldn't parse input: %s, %s\n",
			json_tokener_error_desc(jerr), buf);
		return 1;
	}
	json_tokener_free(tok);


	/* Traverse parsed JSON breadth-first
	 * descending for every "-" and only the matched keys if given */
	
	enqueue(queue, jobj);
	enqueue(queue, QSEP);

	int c;
	for (int i = 0; i < key_len; i++) {
		while((jobj = dequeue(queue)) != QSEP) {
			if (strcmp("-", key[i]) == 0) {
				enqueue_all(queue, jobj);
			} else {
				for (int j=i; j < key_len; j++) {
					if (strcmp("-", key[j]) == 0)
						break;
					enqueue(queue, get_element(jobj, key[j]));
					c++;
				}
			}
		}
		i = i + c;
		c = 0;
		enqueue(queue, QSEP);	
	}


	/* Print out all leafs of the match tree */

	while (queue->head != NULL) {
		if ((jobj = dequeue(queue)) != QSEP) {
			if (IgnoreNullValues || jobj != NULL)
				print_element(jobj);
		}
	}

	return 0;
}
