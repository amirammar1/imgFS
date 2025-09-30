#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include "util.h"   // for _unused


#include "http_prot.h"
#include "http_net.h"
#include "socket_layer.h"
#include "error.h"


/*******************************************************************
 * Checks whether the `message` URI starts with the provided `target_uri`
 */
int http_match_uri(const struct http_message *message, const char *target_uri)
{
    M_REQUIRE_NON_NULL(message);
    M_REQUIRE_NON_NULL(message->uri.val);
    M_REQUIRE_NON_NULL(target_uri);

    size_t length_prefix = strlen(target_uri);
    if(length_prefix > message->uri.len)return 0;
    return strncmp(message->uri.val,target_uri,length_prefix)==0 ? 1 : 0;
}

/*******************************************************************
 * Compare method with verb and return 1 if they are equal, 0 otherwise
 */
int http_match_verb(const struct http_string* method, const char* verb)
{
    M_REQUIRE_NON_NULL(method);
    M_REQUIRE_NON_NULL(verb);
    if(method->len != strlen(verb))return 0;
    return strncmp(method->val,verb,method->len)==0 ? 1 : 0;
}

/*******************************************************************
 * Writes the value of parameter `name` from URL in message to buffer out
 */
int http_get_var(const struct http_string* url, const char* name, char* out, size_t out_len)
{
    M_REQUIRE_NON_NULL(url);
    M_REQUIRE_NON_NULL(url->val);
    M_REQUIRE_NON_NULL(name);
    size_t size_new_name = strlen(name) + 1;
    char new_name[size_new_name + 1];
    strcpy(new_name,name);
    strcat(new_name,"=");
    char url_str[url->len + 1];
    strncpy(url_str,url->val,url->len);
    url_str[url->len] = '\0';

    //This is a small bonus we added to check more that URL is valid
    char* pos_bonus = strstr(url_str,"?");
    if(pos_bonus == NULL)return ERR_INVALID_ARGUMENT;
    char* position_name_pref = strstr(pos_bonus,new_name);
    if(position_name_pref==NULL)return 0;
    if(*(position_name_pref-1) != '?' && *(position_name_pref-1) != '&')return ERR_INVALID_ARGUMENT;



    char* position_name = position_name_pref + size_new_name;
    if(*position_name== '\0')return ERR_RUNTIME;
    char* position_end = strstr(position_name,"&");
    size_t real_size;
    if(position_end==NULL) {
        real_size = url->len - (size_t) (position_name - url_str);
    } else real_size = (size_t) (position_end - position_name);
    if(real_size>out_len) {
        return ERR_RUNTIME;
    }

    strncpy(out,position_name,real_size);
    out[real_size] = '\0';
    return (int)real_size;
}

static const char* get_next_token(const char* message, const char* delimiter, struct http_string* output)
{
    char* delim_pos = strstr(message,delimiter);
    if(delim_pos == NULL) {
        if(output!=NULL) {
            output->val = message;
            output->len = strlen(message);
        }
        return NULL;
    }
    if(output!=NULL) {
        output->len =(size_t) (delim_pos - message);
        output->val = message;
    }
    return delim_pos + strlen(delimiter);
}

static const char* http_parse_headers(const char* header_start, struct http_message* output, int* content_len)
{
    size_t head_line = 0;
    while (head_line<MAX_HEADERS) {
        if(header_start == NULL)break;
        if(strncmp(header_start,HTTP_LINE_DELIM, strlen(HTTP_LINE_DELIM))==0)break;
        header_start = get_next_token(header_start,HTTP_HDR_KV_DELIM,&output->headers[head_line].key);
        header_start = get_next_token(header_start,HTTP_LINE_DELIM,&output->headers[head_line].value);

        if(http_match_verb(&output->headers[head_line].key,"Content-Length")) {
            char length_str[output->headers[head_line].value.len+1];
            strncpy(length_str,output->headers[head_line].value.val,output->headers[head_line].value.len);
            length_str[output->headers[head_line].value.len] = '\0';
            int length = (int)atouint32(length_str);
            *content_len = length;
        }
        head_line++;
    }
    output->num_headers = head_line;
    return header_start;
}

/*******************************************************************
 * Assumes that all characters of stream that are not filled by reading are set to 0
 */
int http_parse_message(const char *stream, size_t bytes_received, struct http_message *out, int *content_len)
{
    M_REQUIRE_NON_NULL(stream);
    M_REQUIRE_NON_NULL(out);
    M_REQUIRE_NON_NULL(content_len);
    const char* stream_cpy = stream;
    char* end_head = strstr(stream_cpy,HTTP_HDR_END_DELIM);
    if(end_head == NULL)return 0;
    stream_cpy = get_next_token(stream_cpy," ",&out->method);
    if(out->method.len ==0)return ERR_INVALID_ARGUMENT;
    stream_cpy = get_next_token(stream_cpy," ",&out->uri);
    stream_cpy = get_next_token(stream_cpy,HTTP_LINE_DELIM,NULL);

    *content_len = 0;
    const char* stream_parse = http_parse_headers(stream_cpy,out,content_len);
    char* body = strstr(stream_parse,HTTP_LINE_DELIM);
    if(body == NULL)return 0;
    if(*content_len == 0)return 1;
    body+= strlen(HTTP_LINE_DELIM);
    out->body.len = bytes_received - (size_t) (body - stream);
    out->body.val = body;
    if(out->body.len != (size_t) *content_len)return 0;
    return 1;
}
