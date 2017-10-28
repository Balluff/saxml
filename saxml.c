/* \copyright 2017 Zorxx Software. All rights reserved.
 * \license This file is released under the MIT License. See the LICENSE file for details.
 * \brief Embedded XML Parser
 */
#include <malloc.h>
#include "saxml.h"

typedef void (*pfnParserStateHandler)(void *context, const uint8_t character);

typedef struct
{
    tSaxmlContext *user;
    
    pfnParserStateHandler pfnHandler;

    int bInitialize; /* true for first call into a state */

    uint8_t *buffer;
    uint32_t maxStringSize;
    uint32_t length;
} tParserContext; 

static void state_Begin(void *context, const uint8_t character);
static void state_StartTag(void *context, const uint8_t character);
static void state_TagName(void *context, const uint8_t character);
static void state_TagContents(void *context, const uint8_t character);
static void state_EndTag(void *context, const uint8_t character);
static void state_EmptyTag(void *context, const uint8_t character);
static void state_Attribute(void *context, const uint8_t character);

#if !defined(DBG)
    #define DBG(...)
#endif

#define ChangeState(ctxt, state) \
    (ctxt)->pfnHandler = state;  \
    (ctxt)->bInitialize = 1;

#define ContextBufferAddChar(ctxt, character)        \
    if((ctxt)->length < ((ctxt)->maxStringSize) - 2) \
    {                                                \
        (ctxt)->buffer[(ctxt)->length] = character;  \
        ++((ctxt)->length);                          \
    }                                                \
    else { /* string truncated */ }

#define CallHandler(ctxt, handlerName)                                   \
    if(NULL != (ctxt)->user->handlerName && (ctxt)->length > 0)          \
    {                                                                    \
        (ctxt)->buffer[(ctxt)->length] = '\0';                           \
        ++((ctxt)->length);                                              \
        (ctxt)->user->handlerName((ctxt)->user->cookie, (ctxt)->buffer); \
    }

/* ---------------------------------------------------------------------------------------------
 * Exported Functions
 */

tSaxmlParser saxml_Initialize(tSaxmlContext *context, const uint32_t maxStringSize)
{
    tParserContext *ctxt;

    if(maxStringSize < 2)
        return NULL;
    if(NULL == context)
        return NULL;

    ctxt = (tParserContext *) malloc(sizeof(*context));
    if(NULL == ctxt)
        return NULL;

    ctxt->buffer = (uint8_t *) malloc(maxStringSize);
    if(NULL == ctxt->buffer)
    {
        free(ctxt);
        return NULL;
    }

    ctxt->user = context;
    ctxt->length = 0; 
    ctxt->maxStringSize = maxStringSize;
    ChangeState(ctxt, state_Begin);

    return (tSaxmlParser) ctxt;
}

void saxml_Deinitialize(tSaxmlParser parser)
{
    tParserContext *ctxt = (tParserContext *) parser;
    if(NULL != ctxt)
    {
        if(NULL != ctxt->buffer)
            free(ctxt->buffer);
        free(ctxt);
    }
}

void saxml_HandleCharacter(tSaxmlParser parser, const uint8_t character)
{
    tParserContext *ctxt = (tParserContext *) parser;
    ctxt->pfnHandler(ctxt, character);
}

void saxml_Reset(tSaxmlParser parser)
{
    tParserContext *ctxt = (tParserContext *) parser;
    ChangeState(ctxt, state_Begin);
}

/* ---------------------------------------------------------------------------------------------
 * State Handlers
 */

/* Wait for a tag start character */
static void state_Begin(void *context, const uint8_t character)
{
    tParserContext *ctxt = (tParserContext *) context;
    pfnParserStateHandler nextState = NULL;

    DBG("%s: %c\n", __func__, character);

    if(ctxt->bInitialize)
    {
        DBG("%s: Initialize\n", __func__);
        ctxt->length = 0; 
        ctxt->bInitialize = 0;
    }

    switch(character)
    {
        case '<':
           nextState = state_StartTag;
           break;
        default:
            break;
    }

    if(NULL != nextState)
    {
        ChangeState(ctxt, nextState);
    }
}

/* We've already found a tag start character, determine if this is start or end tag,
 *  and parse the tag name */
static void state_StartTag(void *context, const uint8_t character)
{
    tParserContext *ctxt = (tParserContext *) context;

    DBG("%s: %c\n", __func__, character);

    if(ctxt->bInitialize)
    {
        DBG("%s: Initialize\n", __func__);
        ctxt->bInitialize = 0;
    }

    switch(character)
    {
        case '<': case '>':
            /* Syntax error! */
            break;
        case ' ': case '\r': case '\n': case '\t':
            /* Ignore whitespace */
            break;
        case '/':
            ChangeState(ctxt, state_EndTag);
            break;
        default:
            ctxt->buffer[0] = character;
            ctxt->length = 1;
            ChangeState(ctxt, state_TagName);
            break;
    }
}

static void state_TagName(void *context, const uint8_t character)
{
    tParserContext *ctxt = (tParserContext *) context;
    pfnParserStateHandler nextState = NULL;

    DBG("%s: %c\n", __func__, character);

    if(ctxt->bInitialize)
    {
        /* Expect one character in the buffer; the start of the tag name from the previous state*/
        DBG("%s: Initialize\n", __func__);
        ctxt->bInitialize = 0;
    }

    switch(character)
    {
        case ' ': case '\r': case '\n': case '\t':
            /* Tag name complete, whitespace indicates tag attribute */
            nextState = state_Attribute;
            break;
        case '/':
            nextState = state_EmptyTag;
            break;
        case '>':
            nextState = state_TagContents; /* Done with tag, contents may follow */
            break;
        default: 
            ContextBufferAddChar(ctxt, character);
            break;
    }

    if(NULL != nextState)
    {
        CallHandler(ctxt, tagHandler);
        ChangeState(ctxt, nextState);
    }
}

static void state_EmptyTag(void *context, const uint8_t character)
{
    tParserContext *ctxt = (tParserContext *) context;
    pfnParserStateHandler nextState = NULL;

    DBG("%s: %c\n", __func__, character);

    if(ctxt->bInitialize)
    {
        /* We need to keep the buffer as-is, since it contains the tag name */
        DBG("%s: Initialize\n", __func__);
        ctxt->bInitialize = 0;
    }

    switch(character)
    {
        case '>':
            nextState = state_TagContents;
            break;
        default:
            break;
    }

    if(NULL != nextState)
    {
        CallHandler(ctxt, tagEndHandler);
        ChangeState(ctxt, nextState);
    }
}

static void state_TagContents(void *context, const uint8_t character)
{
    tParserContext *ctxt = (tParserContext *) context;
    pfnParserStateHandler nextState = NULL;

    DBG("%s: %c\n", __func__, character);

    if(ctxt->bInitialize)
    {
        DBG("%s: Initialize\n", __func__);
        ctxt->length = 0;
        ctxt->bInitialize = 0;
    }

    switch(character)
    {
        case '<':
            nextState = state_StartTag;
            break;
        case ' ': case '\r': case '\n': case '\t':
            if(0 == ctxt->length)
                break; /* Ignore leading whitespace */
            else
            {
                /* FALLTHRU */
            }
        default:
            ContextBufferAddChar(ctxt, character);
            break;
    }

    if(NULL != nextState)
    {
        CallHandler(ctxt, contentHandler);
        ChangeState(ctxt, nextState);
    }
}

static void state_Attribute(void *context, const uint8_t character)
{
    tParserContext *ctxt = (tParserContext *) context;
    pfnParserStateHandler nextState = NULL;

    DBG("%s: %c\n", __func__, character);

    if(ctxt->bInitialize)
    {
        DBG("%s: Initialize\n", __func__);
        ctxt->length = 0;
        ctxt->bInitialize = 0;
    }

    switch(character)
    {
        case ' ': case '\r': case '\n': case '\t':
            if(0 == ctxt->length)
                break;
            else
                nextState = state_Attribute;
            break;
        case '/':
            nextState = state_EmptyTag;
            break;
        case '>':
            nextState = state_TagContents; /* Done with tag, contents may follow */
            break;
        default:
            ContextBufferAddChar(ctxt, character);
            break;
    }

    if(NULL != nextState)
    {
        CallHandler(ctxt, attributeHandler);
        ChangeState(ctxt, nextState);
    }
}

static void state_EndTag(void *context, const uint8_t character)
{
    tParserContext *ctxt = (tParserContext *) context;
    pfnParserStateHandler nextState = NULL;

    DBG("%s: %c\n", __func__, character);

    if(ctxt->bInitialize)
    {
        DBG("%s: Initialize\n", __func__);
        ctxt->length = 0;
        ctxt->bInitialize = 0;
    }

    switch(character)
    {
        case '<':
            /* Syntax error! */
            break;
        case ' ': case '\r': case '\n': case '\t':
            /* Ignore whitespace */
            break;
        case '>':
            nextState = state_TagContents;
            break;
        default:
            ContextBufferAddChar(ctxt, character);
            break;
    }

    if(NULL != nextState)
    {
        CallHandler(ctxt, tagEndHandler);
        ChangeState(ctxt, nextState);
    }
}
