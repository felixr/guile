/*
 * Copyright (C) 2013  Free Software Foundation, Inc.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Authors:
 *	Paulo Cesar Pereira de Andrade
 */

#include <lightning.h>
#include <lightning/jit_private.h>

/*
 * Prototypes
 */
#define new_note(u, v)		_new_note(_jit, u, v)
static jit_note_t *_new_note(jit_state_t *, jit_uint8_t*, char*);
static void new_line(jit_int32_t,jit_note_t*,char*,jit_int32_t,jit_int32_t);
#define note_search_index(u)	_note_search_index(_jit, u)
static jit_int32_t _note_search_index(jit_state_t*, jit_uint8_t*);
static jit_int32_t line_insert_index(jit_note_t*,jit_int32_t);
static jit_int32_t line_search_index(jit_note_t*,jit_int32_t);
static jit_int32_t offset_insert_index(jit_line_t*,jit_int32_t);
static jit_int32_t offset_search_index(jit_line_t*,jit_int32_t);

/*
 * Implementation
 */
void
jit_init_note(void)
{
}

void
jit_finish_note(void)
{
}

jit_node_t *
_jit_name(jit_state_t *_jit, char *name)
{
    jit_node_t		*node;

    node = jit_new_node(jit_code_name);
    if (name)
	node->v.n = jit_data(name, strlen(name) + 1, 1);
    else
	node->v.p = NULL;
    if (_jit->note.head == NULL)
	_jit->note.head = _jit->note.tail = node;
    else {
	_jit->note.tail->link = node;
	_jit->note.tail = node;
    }

    return (node);
}

jit_node_t *
_jit_note(jit_state_t *_jit, char *name, int line)
{
    jit_node_t		*node;

    node = jit_new_node(jit_code_note);
    if (name)
	node->v.n = jit_data(name, strlen(name) + 1, 1);
    else
	node->v.p = NULL;
    node->w.w = line;
    if (_jit->note.head == NULL)
	_jit->note.head = _jit->note.tail = node;
    else {
	_jit->note.tail->link = node;
	_jit->note.tail = node;
    }

    return (node);
}

void
_jit_annotate(jit_state_t *_jit)
{
    jit_node_t		*node;
    jit_note_t		*note;

    note = NULL;
    for (node = _jit->note.head; node; node = node->link) {
	if (node->code == jit_code_name)
	    note = new_note(node->u.p, node->v.p ? node->v.n->u.p : NULL);
	else if (node->v.p) {
	    if (note == NULL)
		note = new_note(node->u.p, NULL);
	    jit_set_note(note, node->v.n->u.p, node->w.w,
			 (jit_uint8_t *)node->u.p - note->code);
	}
    }
    /* last note */
    if (note)
	note->size = _jit->pc.uc - note->code;
}

void
_jit_set_note(jit_state_t *_jit, jit_note_t *note,
	      char *file, int lineno, jit_int32_t offset)
{
    jit_line_t		*line;
    jit_int32_t		 index;

    index = line_insert_index(note, offset);
    if (index >= note->length || note->lines[index].file != file)
	new_line(index, note, file, lineno, offset);
    else {
	line = note->lines + index;
	index = offset_insert_index(line, offset);
	if (line->offsets[index] == offset) {
	    /* common case if no code was generated for several source lines */
	    if (line->linenos[index] < lineno)
		line->linenos[index] = lineno;
	}
	else if (line->linenos[index] == lineno) {
	    /* common case of extending entry */
	    if (line->offsets[index] > offset)
		line->offsets[index] = offset;
	}
	else {
	    /* line or offset changed */
	    if ((line->length & 15) == 0) {
		line->linenos = realloc(line->linenos, (line->length + 17) *
					sizeof(jit_int32_t));
		line->offsets = realloc(line->offsets, (line->length + 17) *
					sizeof(jit_int32_t));
	    }
	    if (index < note->length) {
		memmove(line->linenos + index + 1, line->linenos + index,
			sizeof(jit_int32_t) * (line->length - index));
		memmove(line->offsets + index + 1, line->offsets + index,
			sizeof(jit_int32_t) * (line->length - index));
	    }
	    line->linenos[index] = lineno;
	    line->offsets[index] = offset;
	    ++line->length;
	}
    }
}

jit_bool_t
_jit_get_note(jit_state_t *_jit, jit_uint8_t *code,
	      char **name, char **file, jit_int32_t *lineno)
{
    jit_note_t		*note;
    jit_line_t		*line;
    jit_int32_t		 index;
    jit_int32_t		 offset;

    if ((index = note_search_index(code)) >= _jit->note.length)
	return (0);
    note = _jit->note.ptr + index;
    if (code < note->code || code >= note->code + note->size)
	return (0);
    offset = code - note->code;
    if ((index = line_search_index(note, offset)) >= note->length)
	return (0);
    if (index == 0 && offset < note->lines[0].offsets[0])
	return (0);
    line = note->lines + index;
    if ((index = offset_search_index(line, offset)) >= line->length)
	return (0);

    if (name)
	*name = note->name;
    if (file)
	*file = line->file;
    if (lineno)
	*lineno = line->linenos[index];

    return (1);
}

static jit_note_t *
_new_note(jit_state_t *_jit, jit_uint8_t *code, char *name)
{
    jit_note_t		*note;
    jit_note_t		*prev;

    if (_jit->note.ptr == NULL) {
	prev = NULL;
	_jit->note.ptr = malloc(sizeof(jit_note_t) * 8);
    }
    else {
	if ((_jit->note.length & 7) == 7)
	    _jit->note.ptr = realloc(_jit->note.ptr, sizeof(jit_note_t) *
				     (_jit->note.length + 9));
	prev = _jit->note.ptr + _jit->note.length - 1;
    }
    if (prev) {
	assert(code >= prev->code);
	prev->size = code - prev->code;
    }
    note = _jit->note.ptr + _jit->note.length;
    ++_jit->note.length;
    note->code = code;
    note->name = name;
    note->lines = NULL;
    note->length = note->size = 0;

    return (note);
}

static void
new_line(jit_int32_t index, jit_note_t *note,
	  char *file, jit_int32_t lineno, jit_int32_t offset)
{
    jit_line_t		*line;

    if (note->lines == NULL)
	note->lines = malloc(16 * sizeof(jit_line_t));
    else if ((note->length & 15) == 15)
	note->lines = realloc(note->lines,
			      (note->length + 17) * sizeof(jit_line_t));

    if (index < note->length)
	memmove(note->lines + index + 1, note->lines + index,
		sizeof(jit_line_t) * (note->length - index));
    line = note->lines + index;
    ++note->length;

    line->file = file;
    line->length = 1;
    line->linenos = malloc(16 * sizeof(jit_int32_t));
    line->linenos[0] = lineno;
    line->offsets = malloc(16 * sizeof(jit_int32_t));
    line->offsets[0] = offset;
}

static jit_int32_t
_note_search_index(jit_state_t *_jit, jit_uint8_t *code)
{
    jit_int32_t		 bot;
    jit_int32_t		 top;
    jit_int32_t		 index;
    jit_note_t		*notes;

    bot = 0;
    top = _jit->note.length;
    notes = _jit->note.ptr;
    for (index = (bot + top) >> 1; bot < top; index = (bot + top) >> 1) {
	if (code < notes[index].code)
	    top = index;
	else if (code >= notes[index].code &&
		 code - notes[index].code < notes[index].size)
	    break;
	else
	    bot = index + 1;
    }

    return (index);
}

static jit_int32_t
line_insert_index(jit_note_t *note, jit_int32_t offset)
{
    jit_int32_t		 bot;
    jit_int32_t		 top;
    jit_int32_t		 index;
    jit_line_t		*lines;

    bot = 0;
    top = note->length;
    if ((lines = note->lines) == NULL)
	return (0);
    for (index = (bot + top) >> 1; bot < top; index = (bot + top) >> 1) {
	if (offset < *lines[index].offsets)
	    top = index;
	else
	    bot = index + 1;
    }

    return ((bot + top) >> 1);
}

static jit_int32_t
line_search_index(jit_note_t *note, jit_int32_t offset)
{
    jit_int32_t		 bot;
    jit_int32_t		 top;
    jit_int32_t		 index;
    jit_line_t		*lines;

    bot = 0;
    top = note->length;
    if ((lines = note->lines) == NULL)
	return (0);
    for (index = (bot + top) >> 1; bot < top; index = (bot + top) >> 1) {
	if (offset < *lines[index].offsets)
	    top = index;
	/* offset should be already verified to be in range */
	else if (index == note->length - 1 ||
		 (offset >= *lines[index].offsets &&
		  offset < *lines[index + 1].offsets))
	    break;
	else
	    bot = index + 1;
    }

    return (index);
}

static jit_int32_t
offset_insert_index(jit_line_t *line, jit_int32_t offset)
{
    jit_int32_t		 bot;
    jit_int32_t		 top;
    jit_int32_t		 index;
    jit_int32_t		*offsets;

    bot = 0;
    top = line->length;
    offsets = line->offsets;
    for (index = (bot + top) >> 1; bot < top; index = (bot + top) >> 1) {
	if (offset < offsets[index])
	    top = index;
	else
	    bot = index + 1;
    }

    return ((bot + top) >> 1);
}

static jit_int32_t
offset_search_index(jit_line_t *line, jit_int32_t offset)
{
    jit_int32_t		 bot;
    jit_int32_t		 top;
    jit_int32_t		 index;
    jit_int32_t		*offsets;

    bot = 0;
    top = line->length;
    offsets = line->offsets;
    for (index = (bot + top) >> 1; bot < top; index = (bot + top) >> 1) {
	if (offset < offsets[index])
	    top = index;
	/* offset should be already verified to be in range */
	else if (index == line->length - 1 ||
		 (offset >= offsets[index] && offset < offsets[index + 1]))
	    break;
	else
	    bot = index + 1;
    }

    return (index);
}
