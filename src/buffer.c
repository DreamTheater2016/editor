/*
    editor is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    editor is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with editor.  If not, see <http://www.gnu.org/licenses/>.
*/


/*AUTHOR: SAXON SUPPLE*/


#include <buffer.h>

FILE_BUFFER* init_buffer(char *input_file)
{
	FILE *in_filp = fopen(input_file, "r");
	if (!in_filp)
		return NULL;

	FILE_BUFFER *ret = (FILE_BUFFER*)malloc(sizeof(FILE_BUFFER));

	init_settings(&ret->settings);

	ret->name_len = strlen(input_file);
	ret->name = (char*)malloc(ret->name_len);
	strncpy(ret->name, input_file, ret->name_len);

	ret->status = (struct stat*)malloc(sizeof(struct stat));
	if (stat(input_file, ret->status) == -1)
		return NULL;

	ret->buffer_size = ret->status->st_size;

	ret->file_buf = malloc(ret->buffer_size);
	fread(ret->file_buf, 1, ret->buffer_size, in_filp);
	/*ret->file_buf = mmap(NULL, ret->status->st_size,
			     PROT_READ | PROT_WRITE, MAP_SHARED, fileno(in_filp), 0);*/

	ret->add_buf = (BUFFER*)malloc(sizeof(BUFFER));
	ret->add_buf->size = PAGE_SIZE;
	ret->add_buf->offset = 0;
	//ret->add_buf->head = ret->add_buf->tail = (PAGE*)malloc(sizeof(PAGE));
	//memset(ret->add_buf->head, 0, sizeof(PAGE));
	//posix_memalign((void**)&ret->add_buf->head->page, PAGE_SIZE, PAGE_SIZE);
	ret->add_buf->buffer = (char*)malloc(PAGE_SIZE);

	ret->piece_desc = (PIECE_DESC*)malloc(sizeof(PIECE_DESC));
	ret->piece_desc->tree = RBTreeCreate(piece_compare, key_destroy, info_destroy);
	memset(ret->piece_desc->cache, 0, 3 * sizeof(P_CACHE));

	/*PIECE *nill = (PIECE*)malloc(sizeof(PIECE));
	nill->file = ret;
	nill->flags = INFO | KEY | NILL;
	nill->offset = 0;
	nill->size_left = 0;
	nill->size_right = ret->status->st_size;
	nill->size = 0;
	nill->node = RBTreeInsert(ret->piece_desc->tree, (void*)nill, (void*)nill);
	
	change_current(nill, ret);*/

	PIECE *first = (PIECE*)malloc(sizeof(PIECE));
	first->file = ret;
	first->flags = IN_FILE | INFO | KEY;
	first->offset = 0;
	first->size_left = 0;
	first->size_right = 0;
	first->size = ret->status->st_size;
/*	first->node = (rb_red_blk_node*)malloc(sizeof(rb_red_blk_node));
	first->node->info = first->node->key = (void*)first;
	first->node->parent = nill->node;
	first->node->red = 1;
	first->node->left = ret->piece_desc->tree->nil;
	first->node->right = ret->piece_desc->tree->nil;
	nill->node->right = first->node;*/
	first->node = RBTreeInsert(ret->piece_desc->tree, (void*)first, (void*)first);
	//first->node = insert_left(ret->piece_desc->tree, nill->node);
	change_current(first, ret);
	//nill->node->left = ret->piece_desc->tree->nil;
	ret->win_desc = (WIN_DESC*)malloc(sizeof(WIN_DESC));

	ret->lines = (LINE_TABLE*)malloc(sizeof(LINE_TABLE));
	memset(ret->lines, 0, sizeof(LINE_TABLE));

	LINE_TABLE *table = ret->lines;
	term_get_win_size(&table->rows, &table->cols);
	table->lines_count = table->rows + LINE_CACHE_SIZE * 2;
	//table->used = 22;
	table->lines = (LINE*)malloc(sizeof(LINE) * table->lines_count);
	memset(table->lines, 0, sizeof(LINE) * table->lines_count);
	table->lines[0].start_piece = table->lines[0].end_piece = first;

	ret->rendered = (char*)malloc(table->rows * table->cols);

	init_tui(ret);

	fill_lines(ret, 1);

	table->total_lines = get_line_count(ret);

	ret->filp = in_filp;

	ret->dirty = 0;

	return ret;
}

int save_buffer(FILE_BUFFER *buffer)
{
	FILE *fp = buffer->filp;
	for (PIECE *piece = GET_FIRST_PIECE(buffer); piece != NULL; piece = GET_NEXT_PIECE(piece, buffer))
	{
		fwrite(GET_BUFFER(buffer, piece->flags)+piece->offset, 1, piece->size, fp);
	}
}


void init_settings(SETTINGS *settings)
{
	settings->tab_length = 8;
}

void release_buffer(FILE_BUFFER *buffer)
{
	if (!buffer)
		return;

	//munmap(buffer->file_buf, buffer->status->st_size);

	free(buffer->file_buf);
	BUFFER *add_buf = ADD_BUF(buffer);

	free(add_buf->buffer);
	free(add_buf);

	free(buffer->status);

	free(buffer->name);

	/*free(buffer->user_cache->buffer);
	free(buffer->user_cache);*/

        RBTreeDestroy(buffer->piece_desc->tree);
	free(buffer->piece_desc);

	end_tui(buffer->win_desc);

	free(buffer->win_desc);

	//free(buffer->lines->span1);

	free(buffer->lines);

	free(buffer->rendered);
	fclose(buffer->filp);
	free(buffer);
}
/*
void clear_user_cache(FILE_BUFFER *buffer)
{
	BUFFER *cache = buffer->user_cache;
	memset(cache->buffer, 0, cache->size);
	cache->offset = 0;
}

void user_cache_append(const char new_item, FILE_BUFFER *buffer)
{
	BUFFER *cache = buffer->user_cache;
	if (cache->offset == cache->size)
	{
		cache->size += 10;
		cache->buffer = (char*)realloc((void*)cache->buffer, cache->size);
	}
	cache->buffer[cache->offset++] = new_item;
}*/



size_t add_buffer_append(const char *new_item, size_t len, FILE_BUFFER *buffer)
{
	if (buffer == NULL || new_item == NULL || len == 0)
		return 0;

	BUFFER *add_buf = ADD_BUF(buffer);

	//PAGE *current = add_buf->tail;

	size_t wrote = 0;
	register const char *ptr = new_item;

	while (len > 0)
	{
		if (add_buf->size == add_buf->offset)
		{
			add_buf->buffer = (char*)realloc(add_buf->buffer, add_buf->size += len + PAGE_SIZE);
		}

		while (add_buf->offset < add_buf->size && len > 0)
		{
			add_buf->buffer[add_buf->offset++] = *(ptr++);
			++wrote;
			--len;
		}
	}

	return wrote;
}



PIECE *find_containing_piece(size_t offset, FILE_BUFFER *buffer, size_t *ret_off)
{
	/*P_CACHE *tmp = find_in_cache_off(offset, buffer);
	  if (tmp != NULL)
	  return tmp->piece;*/
	//TODO: situation when len overlaps multiple pieces
	rb_red_blk_node *ret = buffer->piece_desc->tree->root->left;

	while (ret != buffer->piece_desc->tree->nil)
	{
		size_t cur_off = piece_offset((PIECE*)ret->info);
		if (cur_off < offset)
		{
			if (cur_off + ((PIECE*)ret->info)->size > offset/* + len*/)
			{
				if (ret_off != NULL)
					*ret_off = cur_off;
				return (PIECE*)ret->info;
			}
			ret = ret->right;
		}
		else if (cur_off > offset)
		{
			ret = ret->left;
		}
		else if (((PIECE*)ret->info)->size > 0)
		{
			if (ret_off != NULL)
				*ret_off = cur_off;
			return (PIECE*)ret->info;
		}
		else
			ret = ret->left;
	}
	return NULL;
}


int insert_item(const char *new_item, size_t len, size_t offset, FILE_BUFFER *buffer)
{
	size_t off;
	PIECE *cur_piece = find_containing_piece(offset, buffer, &off);

	size_t old;

	if (cur_piece == NULL)
	{
		//return -1;
#ifdef DEBUG_ASSERT
		assert(offset != 0);
#endif

		cur_piece = find_containing_piece(offset-1, buffer, &off);

		if (cur_piece != NULL)
		{
			change_current(cur_piece, buffer);
			if (P_INADD(cur_piece) && cur_piece->size + cur_piece->offset == ADD_BUF(buffer)->offset)
				goto redo;
			piece_insert_right(len, IN_ADD, ADD_BUF(buffer)->offset, buffer);
		}
		else
		{
			cur_piece = find_containing_piece(offset + 1, buffer, &off);

#ifdef DEBUG_ASSERT
			assert(cur_piece != NULL);
#endif
			change_current(cur_piece, buffer);
			piece_insert_left(len, IN_ADD, ADD_BUF(buffer)->offset, buffer);
		}
		goto exit;
	}

	if (P_INADD(cur_piece) && cur_piece->size + off == offset
	    && cur_piece->size + cur_piece->offset == ADD_BUF(buffer)->offset)
	{
	redo:
		/*if (piece->flags & NILL)
		{
			piece->flags && 
			}*/
	        old = cur_piece->size;
		cur_piece->size += len;
		fix_sizes(cur_piece, len);
	}
/*	else if (offset - off == 0 && piece_offset(cur_piece) > 0 &&
		 cur_piece->size + cur_piece->offset == ADD_BUF(buffer)->offset)
	{
	        /*cur_piece = GET_PREV_PIECE(cur_piece, buffer);
		if (P_INADD(cur_piece) && cur_piece->size + cur_piece->offset == ADD_BUF(buffer)->offset)
			goto redo;
		cur_piece = GET_NEXT_PIECE(cur_piece, buffer);
		goto fail;*
		
	}*/
	else
	{
	fail:
		change_current(cur_piece, buffer);
	        old = cur_piece->size;
		size_t old_off = cur_piece->offset;
		char old_flags = cur_piece->flags;
		size_t old_pos = off;
		size_t new = offset - off;
		if (new > 0)
		{
			cur_piece->size = new;
			fix_sizes(cur_piece, (int)new - (int)old);

			cur_piece = piece_insert_right(len, IN_ADD, ADD_BUF(buffer)->offset, buffer);
			change_current(cur_piece, buffer);
			piece_insert_right(/*old - offset - len*/old-new, old_flags, old_off + new, buffer);
		}
		else
		{
			//	if (cur_piece->flags & NILL == 0)
			{	cur_piece = GET_PREV_PIECE(cur_piece, buffer);
			off -= cur_piece->size;
			goto fail;
			}
			/*	else
			{
				cur_piece->flags &= ~NILL;
				piece_insert_left(len, IN_ADD, ADD_BUF(buffer)->offset, buffer);
				}*/
		}
		
	}
	add_buffer_append(new_item, len, buffer);

exit:
	buffer->buffer_size += len;

	return 0;
}



/*
  To delete an item:
  Create a piece pointing to the items before the deleted one
  Create a piece pointing to the items after the deleted one
*/
int delete_item(size_t offset, size_t len/*amount to delete*/, FILE_BUFFER *buffer)
{
	size_t abs_off;
	PIECE *piece = find_containing_piece(offset, buffer, &abs_off);

	if (piece == NULL)
		return -1;
	change_current(piece, buffer);
	if (offset == abs_off)
	{
		if (len < piece->size)
		{
			piece->size -= len;
			fix_sizes(piece, -(int)len);
			piece->offset += len;
		}
		else if (len == piece->size)
		{
			//fix_sizes(piece, -len);
			delete_piece(piece, buffer);
		}
		else
		{
			return -1;
		}
		goto exit;
	}
	if (offset + len == abs_off + piece->size)
	{
		piece->size -= len;
		fix_sizes(piece, -(int)len);
		//piece->offset += len;
	        goto exit;
	}
	size_t old = piece->size;
	piece->size = offset - abs_off;
	fix_sizes(piece, -((int)piece->size - (int)old));

	piece_insert_right(old-piece->size-len,piece->flags,piece->offset+piece->size+len,buffer);
exit:
	buffer->buffer_size -= len;
	return 0;
}

static void delete_piece(PIECE *piece, FILE_BUFFER *buffer/*, char flags*/)
{
	fix_sizes(piece, -(int)piece->size);
	RBDelete(buffer->piece_desc->tree, piece->node);
}



size_t piece_offset(const PIECE *key)
{
	size_t offset;
	/*if ((offset = find_offset_in_cache(key, key->file)) != MAGIC)
		return offset;*/
	offset = key->size_left;
        rb_red_blk_node *node = key->node;

	while (!IS_NIL(node->parent))
	{
		if (node->parent->right == node)
		{
			PIECE *p_info = (PIECE*)node->parent->info;
			offset += p_info->size_left +
			        p_info->size;
		}

		node = node->parent;
	}

	return offset;
}


/*used for the red-black tree*/
int piece_compare(const void *a, const void *b)
{
	const PIECE *_a = (PIECE*)a;
	const PIECE *_b = (PIECE*)b;
	size_t off1 = piece_offset(_a);
	size_t off2 = piece_offset(_b);
	if (off1 > off2)
		return 1;
	if (off1 < off2)
		return -1;
	return 0;
}

void info_destroy(void *info)
{
	PIECE *_info = (PIECE*)info;
	if ((_info->flags & KEY) == 0)
	{
		free(_info);
	}
	else
	{
		_info->flags &= ~INFO;
	}
}

void key_destroy(void *key)
{
	PIECE *_key = (PIECE*)key;
	if ((_key->flags & INFO) == 0)
	{
		free(_key);
	}
	else
	{
		_key->flags &= ~KEY;
	}
}



/*
  For when the size field in a piece changes.
  Need to update the size_left field of all its right parents.
 */
void fix_sizes(PIECE *piece, int change)
{
	rb_red_blk_node *node = piece->node;
	if (change == 0 || node->parent == NULL)
		return;
	while (!IS_ROOT(node/*->parent*/))
	{
		if (node->parent->left == node)
		{
			piece = (PIECE*)node->parent->info;
			piece->size_left += change;
		}
		else
		{
			piece = (PIECE*)node->parent->info;
			piece->size_right += change;
		}

		node = node->parent;
	}
}



void print_buffer(FILE_BUFFER *buffer)
{
	PIECE_DESC *desc = buffer->piece_desc;

	PIECE *piece = (PIECE*)TreeMin(desc->tree)->info;

	char *add_buf = buffer->add_buf->buffer;

        while (piece != NULL)
	{
		if ((piece->flags & 8) == 8)
		{
		        for (size_t off = 0; off < piece->size; ++off)
			{
				printf("%c", buffer->file_buf[piece->offset + off]);
			}
		}
		else
		{
			/*PAGE *current = buffer->add_buf->head;
			for (size_t off = 0; off < piece->size;)
			{
				printf("%c", current->page[(piece->offset + off) % PAGE_SIZE]);
				if (++off % PAGE_SIZE == 0)
					current = current->next;
					}*/
			for (size_t off = 0; off < piece->size; ++off)
			{
				printf("%c", add_buf[piece->offset + off]);
			}
		}
	        piece = (PIECE*)(TreeSuccessor(desc->tree, piece->node)->info);
	}
}



ssize_t buffer_read(char *dest, size_t offset, size_t count, FILE_BUFFER *buffer)
{
	size_t abs_off;
	PIECE *container = find_containing_piece(offset, buffer, &abs_off);
#ifdef DEBUG_ASSERT
	Assert(container != NULL, "In get_string. container == NULL");
#endif
	ssize_t read = 0;
	int chars;
	register char *ptr;
	//size_t piece_off = abs_off + container->size - offset;
	size_t piece_off = offset - abs_off;
	size_t old_size;
	while (count > 0)
	{
		//container = find_containing_piece(offset + count, buffer, &abs_off);
#ifdef DEBUG_ASSERT
		Assert(container != NULL, "In get_string. container == NULL");
#endif
		chars = (count < container->size - piece_off) ?
			count : container->size - piece_off;

		read += chars;
		count -= chars;

		ptr = piece_off + GET_BUFFER(buffer, container->flags) + container->offset;
		piece_off = 0;
		old_size = container->size;
		container = (PIECE*)TreeSuccessor(buffer->piece_desc->tree, container->node)->info;
		abs_off += old_size;
#ifdef DEBUG_ASSERT
		Assert(abs_off < buffer->buffer_size, "In get_string: abs_off < buffer->buffer_size");
#endif
		while (chars-- > 0)
			*(dest++) = *(ptr++);
	}
	return read;
}



char buffer_readc(size_t offset, FILE_BUFFER *buffer)
{
	size_t abs_off;
	PIECE *container = find_containing_piece(offset, buffer, &abs_off);

#ifdef DEBUG_ASSERT
	if (container == NULL)
	{
		printf("%d", (int)offset);
		assert(0);
	}
	//Assert(container != NULL, "In buffer_readc. container == NULL");
#endif
	size_t piece_off = offset - abs_off;

	return *(GET_BUFFER(buffer, container->flags) + piece_off + container->offset);
}



PIECE *piece_insert_left(size_t size, char flags, size_t span_off, FILE_BUFFER *buffer)
{
	PIECE *ret = (PIECE*)malloc(sizeof(PIECE));

	ret->file = buffer;

	ret->node = insert_left(buffer->piece_desc->tree, CURRENT(buffer)->node);

	ret->node->info = ret->node->key = ret;

        PIECE *tmp = (PIECE*)ret->node->left->info;

	if (tmp != NULL)
		ret->size_left = tmp->size_left + tmp->size + tmp->size_right;
	else
		ret->size_left = 0;

	tmp = (PIECE*)ret->node->right->info;

	if (tmp != NULL)
	{
		ret->size_right = tmp->size_left + tmp->size + tmp->size_right;
	}
	else
		ret->size_right = 0;

	ret->size = size;

	fix_sizes(ret, size);

	ret->flags = flags | INFO | KEY;
	ret->offset = span_off;


	return ret;
}

PIECE *piece_insert_right(size_t size, char flags, size_t span_off, FILE_BUFFER *buffer)
{
	PIECE *ret = (PIECE*)malloc(sizeof(PIECE));

	ret->file = buffer;

	ret->node = insert_right(buffer->piece_desc->tree, CURRENT(buffer)->node);

	ret->node->info = ret->node->key = ret;

        PIECE *tmp = (PIECE*)ret->node->left->info;

	if (tmp != NULL)
		ret->size_left = tmp->size_left + tmp->size + tmp->size_right;
	else
		ret->size_left = 0;

	tmp = (PIECE*)ret->node->right->info;

	if (tmp != NULL)
	{
		ret->size_right = tmp->size_left + tmp->size + tmp->size_right;
	}
	else
		ret->size_right = 0;

	ret->size = size;

	fix_sizes(ret, size);

	ret->flags = flags | INFO | KEY;
	ret->offset = span_off;


	return ret;
}


/*
  TODO: have a large cache of pieces instead of just
  the current one.
 */
void change_current(PIECE *new_current, FILE_BUFFER *buffer)
{
	//buffer->piece_desc->current = new_current;
	P_CACHE *cache = buffer->piece_desc->cache;
	if (new_current != cache[0].piece)
	{
		/*memset(cache, 0, CACHE_SIZE * sizeof(P_CACHE));
		for (size_t i = 0; i < CACHE_SIZE && new_current != NULL; ++i)
		{
			cache[i].piece = new_current;
			cache[i].offset = (i == 0) ? piece_offset(new_current) :
				cache[i-1].offset + new_current->size;
			new_current = (PIECE*)TreeSuccessor(buffer->piece_desc->tree, new_current->node)->info;
			}*/

		cache[CACHE_CURR].piece = new_current;
		cache[CACHE_CURR].offset = piece_offset(new_current);

		cache[CACHE_PREV].piece = (PIECE*)TreePredecessor(buffer->piece_desc->tree,
								  new_current->node)->info;
		if (cache[CACHE_PREV].piece != NULL)
		{
			cache[CACHE_PREV].offset = cache[CACHE_CURR].offset - cache[CACHE_PREV].piece->size;
		}

		cache[CACHE_NEXT].piece = (PIECE*)TreeSuccessor(buffer->piece_desc->tree,
								  new_current->node)->info;
		if (cache[CACHE_NEXT].piece != NULL)
		{
			cache[CACHE_NEXT].offset = cache[CACHE_CURR].offset + new_current->size;
		}
	}
}

/*
  WARNING: THIS CAN ONLY BE USED WHEN THERE IS AT LEAST 1
  VALID TABLE ENTRY IMMEDIATELY ABOVE IT
 */
size_t fill_lines_offset(FILE_BUFFER *buffer, size_t lineno, size_t table_offset, size_t count)
{
	LINE_TABLE *l_table = buffer->lines;
	PIECE *start = l_table->lines[table_offset-1].end_piece;
	size_t offset = l_table->lines[table_offset-1].end_offset;
	char c;
	size_t len;
	size_t width = l_table->cols;

	if (start != NULL && ++offset >= start->size)
	{
		offset = 0;
		start = GET_NEXT_PIECE(start, buffer);
	}

	for (size_t i = table_offset; start != NULL && count > 0; ++i)
	{
		len = 0;

		l_table->lines[i].start_piece = start;
		l_table->lines[i].start_offset = offset;
		l_table->lines[i].lineno = i + lineno;
		l_table->lines[i].start_abs_offset = piece_offset(start) + offset;		

		while (start != NULL && ((c = piece_read_c(start, start->offset + offset, buffer)) >= 32 || c == 9) && ++len <= width+1)
		{
			if (++offset >= start->size)
			{
				if (start->size == 0)
					--len;
				offset = 0;
				start = GET_NEXT_PIECE(start, buffer);
			}
		}

		l_table->lines[i].end_piece = start;
		l_table->lines[i].end_offset = offset;
		l_table->lines[i].length = len;

		if (start != NULL && ++offset >= start->size)
		{
			offset = 0;
			start = GET_NEXT_PIECE(start, buffer);
		}
		--count;
	}
	return count;
}

void fill_lines(FILE_BUFFER *buffer, size_t lineno)
{
	LINE_TABLE *l_table = buffer->lines;
	PIECE *start;
	size_t offset;
	char c;
	size_t len;
	size_t width = l_table->cols;
	l_table->used_above = (lineno > LINE_CACHE_SIZE) ? LINE_CACHE_SIZE : lineno - 1;
	l_table->start_lineno = lineno - l_table->used_above;
	/*if (l_table->start_lineno == 0 || l_table->start_lineno > lineno)*/
	{
		start = (PIECE*)TreeMin(buffer->piece_desc->tree)->info;
		offset = 0;
		for (size_t i = 1; start != NULL && i < l_table->start_lineno; ++i)
		{
			len = 0;
			while (start != NULL &&
			       (c = piece_read_c(start, start->offset + offset, buffer)) != 10 &&
			       ((c == 9) ? len += buffer->settings.tab_length : ++len) <= width+1)
			{
				if (++offset >= start->size)
				{
					offset = 0;
				        start = GET_NEXT_PIECE(start, buffer);
				}
			}
			if (start == NULL)
				return;
			if (++offset >= start->size)
			{
				offset = 0;
				start = GET_NEXT_PIECE(start, buffer);
			}
			//l_table->lines[i-1].end_piece = end;
		}
	}
	if (start == NULL)
		return;
	//else
	{
		size_t i;
	        for (i = 0; start != NULL && i < l_table->rows+2*LINE_CACHE_SIZE; ++i)
		{
			len = 0;
			l_table->lines[i].start_piece = start;
			l_table->lines[i].start_offset = offset;
			l_table->lines[i].lineno = i + lineno;
			l_table->lines[i].start_abs_offset = piece_offset(start) + offset;

			while (start != NULL && ((c = piece_read_c(start, start->offset + offset, buffer)) >= 32 || c == 9) && ((c == 9) ? len += buffer->settings.tab_length : ++len) < width+1)
			{
				if (++offset >= start->size)
				{
					if (start->size == 0)
						--len;
					offset = 0;
					start = GET_NEXT_PIECE(start, buffer);
				}
			}

			l_table->lines[i].end_piece = start;
			l_table->lines[i].end_offset = offset;
			l_table->lines[i].length = len;

		        if (start != NULL && c == 10 && ++offset >= start->size)
			{
				offset = 0;
				start = GET_NEXT_PIECE(start, buffer);
			}

		}
		i -= l_table->used_above;
		l_table->used = (i > l_table->rows) ? l_table->rows : i;
		l_table->used_bellow = i - l_table->used;
	}
}

int inc_lineno(FILE_BUFFER *buffer)
{
	LINE_TABLE *l_table = buffer->lines;
	if (l_table->used_bellow > 0)
	{
		++l_table->used_above;
		--l_table->used_bellow;
		return 0;
	}
	else if (l_table->total_lines > l_table->start_lineno + l_table->used_above + l_table->used+1)
	{
		size_t cur_line = l_table->start_lineno + l_table->used_above + l_table->used;
	        size_t diff = l_table->total_lines - cur_line;
	        size_t delta = (diff > LINE_CACHE_SIZE) ? LINE_CACHE_SIZE: diff;
		memmove(l_table->lines, l_table->lines + delta, sizeof(LINE) * (l_table->lines_count-delta));
		fill_lines_offset(buffer, cur_line, l_table->used_above+l_table->used-delta, delta);
		l_table->used_above -= delta-1;
		l_table->used_bellow += delta-1;
		l_table->start_lineno += delta-1;
		return 0;
	}
	return -1;
}

//UNOPTOMIZED!!!
void dec_lineno(FILE_BUFFER *buffer)
{
	LINE_TABLE *l_table = buffer->lines;
	if (l_table->used_above > 0)
	{
		--l_table->used_above;
		++l_table->used_bellow;
	}
	else if (l_table->start_lineno > 1)
	{
		size_t diff = l_table->start_lineno - 1;
		size_t delta = (diff > LINE_CACHE_SIZE) ? LINE_CACHE_SIZE : diff;
		//memmove(l_table->lines + delta, l_table->lines, sizeof(LINE) *(l_table->lines_count-delta));
		fill_lines(buffer, l_table->start_lineno - delta);
	}

/*	if (l_table->used_bellow < LINE_CACHE_SIZE && l_table->used_above > 0)
	{
		{
			++l_table->used_bellow;
			--l_table->used_above;
		}
	}
	else if (l_table->used_above > 0)
	{
		memmove(l_table->lines+1, l_table->lines, sizeof(LINE) * (l_table->lines_count-1));

		/*if (l_table->start_lineno > 1)
		{
			fill_lines(buffer, l_table->lines[l_table->start_lineno-1].lineno);
		}
		--l_table->start_lineno;*
		if (l_table->used_above + l_table->used + l_table->used_bellow == l_table->lines_count)
		{
			fill_lines(buffer, l_table->lines[l_table->start_lineno].lineno - 1);
		}
		else
			--l_table->used_above;
		--l_table->start_lineno;
	}*/
}

void add_char_to_line(char c, FILE_BUFFER *buffer)
{
	buffer->dirty = 1;
	LINE_TABLE *l_table = buffer->lines;
	size_t index = LINE_INDEX(l_table, CURSOR_Y(buffer)-1);
	size_t width = WIDTH(l_table);
	//size_t current_lineno = l_table->start_lineno + index;
	insert_item(&c, 1, l_table->lines[index].start_abs_offset + CURSOR_X(buffer)-1, buffer);

	size_t len = 0;
	PIECE *start = l_table->lines[index].start_piece;
	size_t offset = l_table->lines[index].start_offset;
/*	while (start != NULL && ((c = piece_read_c(start, start->offset + offset, buffer)) >= 32 || c == 9) && ++len < width)
	{
		if (++offset >= start->size)
		{
			offset = 0;
			start = GET_NEXT_PIECE(start, buffer);
		}
	}

	l_table->lines[index].end_piece = start;
	l_table->lines[index].end_offset = offset;
*/
/*	if (index > 0)
	fill_lines_offset(buffer, l_table->lines[index].lineno, index, l_table->lines_count - index - 1);*/
//	else
	fill_lines(buffer, l_table->start_lineno+l_table->used_above);
	if (c != 10)
	{
		if (c != 9)
			++buffer->x;
		else
			buffer->x += buffer->settings.tab_length;
		if (buffer->x == width+1)
		{
			if (buffer->y == HEIGHT(l_table) - 1)
			{
				if (l_table->used_bellow > 0)
				{
					inc_lineno(buffer);		
				}
				else
				{
					insert_item("\n", 1, buffer->buffer_size, buffer);
					inc_lineno(buffer);
				}
			}
			++buffer->y;
			buffer->x = 1;
		}
	}
	else
	{
		++buffer->y;
		buffer->x = 1;
	}
/*	else*
	{
		//	++l_table->lines[index].length;
	}*/
}


void get_rendered_output(char *dest, FILE_BUFFER *buffer)
{
	LINE_TABLE *l_table = buffer->lines;
	size_t line = /*(l_table->start_lineno-1) +*/ l_table->used_above;
	PIECE *piece;
	size_t len;
	LINE *lines = l_table->lines;
	size_t cols = l_table->cols;
	size_t size;
	for (size_t i = 0; i < l_table->used; ++i)
	{
		len = 0;
		piece = lines[line].start_piece;
		while (piece != NULL && len < lines[line].length)
		{
			if (piece == lines[line].start_piece)
			{
				if (piece == lines[line].end_piece)
				{
					size = lines[line].end_offset - lines[line].start_offset;
					piece_read(dest + i * cols + len, piece,
						   piece->offset + lines[line].start_offset,
						   size, buffer);
				}
				else
				{
					size = piece->size - lines[line].start_offset;
				        piece_read(dest + i * cols + len, piece,
						   piece->offset + lines[line].start_offset,
						   size, buffer);
					piece = GET_NEXT_PIECE(piece, buffer);
				}
			}
			else if (piece == lines[line].end_piece)
			{
				size = lines[line].end_offset;
				if (size == 0)
					break;
				piece_read(dest + i * cols + len, piece,
					   piece->offset, size, buffer);
			}
			else
			{
				size = piece->size;
				piece_read(dest + i * cols + len, piece, piece->offset, size, buffer);
				piece = GET_NEXT_PIECE(piece, buffer);
			}
			len += size;	
		}
		++line;
	}
}


size_t get_line_count(FILE_BUFFER *buffer)
{
	size_t i, len;
	size_t offset = 0;
	char c;
	size_t width = WIDTH(buffer->lines);
	PIECE *start = GET_FIRST_PIECE(buffer);
	for (i = 0; start != NULL; ++i)
	{
		len = 0;

		while (start != NULL && ((c = piece_read_c(start, start->offset + offset, buffer)) >= 32 || c == 9) && ++len < width)
		{
			if (++offset >= start->size)
			{
				offset = 0;
				start = GET_NEXT_PIECE(start, buffer);
			}
		}

		if (start != NULL && ++offset >= start->size)
		{
			offset = 0;
			start = GET_NEXT_PIECE(start, buffer);
		}
	}
	return i;
}
