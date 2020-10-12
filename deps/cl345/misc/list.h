/****************************************************************************
*																			*
*					  cryptlib List Manipulation Header File 				*
*						Copyright Peter Gutmann 1996-2016					*
*																			*
****************************************************************************/

/* The following functions handle the insertion and deletion of elements to 
   and from singly-linked and doubly-linked lists.  This is the sort of 
   thing that we'd really need templates for, but in their absence we have 
   to use unfortunately rather complex macros.  Where possible these macros 
   are invoked through wrapper functions, limiting the macro code expansion 
   to a small number of locations */

#ifndef _LIST_DEFINED

#define _LIST_DEFINED

/****************************************************************************
*																			*
*					Safe-pointer Single-linked List Functions				*
*																			*
****************************************************************************/

/* Insert and delete elements to/from a single-linked list with safe 
   pointers */

#define insertSingleListElement( listHead, insertPoint, newElement, ELEMENT_TYPE ) \
		{ \
		ELEMENT_TYPE *_listHeadPtr = DATAPTR_GET( listHead ); \
		\
		/* Make sure that the element being added is consistent */ \
		REQUIRES( newElement != NULL ); \
		REQUIRES( DATAPTR_GET( ( newElement )->next ) == NULL ); \
		\
		if( _listHeadPtr == NULL ) \
			{ \
			/* Further consistency check */ \
			REQUIRES( ( insertPoint ) == NULL ); \
			\
			/* It's an empty list, make this the new list */ \
			DATAPTR_SET( listHead, ( newElement ) ); \
			} \
		else \
			{ \
			if( ( insertPoint ) == NULL ) \
				{ \
				/* We're inserting at the start of the list, make this the \
				   new first element */ \
				DATAPTR_SET( ( newElement )->next, _listHeadPtr ); \
				DATAPTR_SET( listHead, ( newElement ) ); \
				} \
			else \
				{ \
				ELEMENT_TYPE *insertPointNext = DATAPTR_GET( ( insertPoint )->next ); \
				\
				/* Insert the element in the middle or the end of the list */ \
				DATAPTR_SET( ( newElement )->next, insertPointNext ); \
				DATAPTR_SET( ( insertPoint )->next, ( newElement ) ); \
				} \
			} \
		}

#define deleteSingleListElement( listHead, listPrev, element, ELEMENT_TYPE ) \
		{ \
		ELEMENT_TYPE *_listHeadPtr = DATAPTR_GET( listHead ); \
		\
		/* Make sure that the preconditions for safe delection are met */ \
		REQUIRES( _listHeadPtr != NULL && element != NULL ); \
		REQUIRES( element == _listHeadPtr || listPrev != NULL ); \
		\
		if( element == _listHeadPtr ) \
			{ \
			/* Special case for the first item */ \
			DATAPTR_SET( listHead, DATAPTR_GET( element->next ) ); \
			} \
		else \
			{ \
			ANALYSER_HINT( listPrev != NULL ); \
			\
			/* Delete from middle or end of the list */ \
			DATAPTR_SET( listPrev->next, DATAPTR_GET( element->next ) ); \
			} \
		DATAPTR_SET( ( element )->next, NULL ); \
		}

/****************************************************************************
*																			*
*					Safe-pointer Double-linked List Functions				*
*																			*
****************************************************************************/

/* Insert and delete elements to/from a double-linked list with safe 
   pointers */

/*								 DATAPTR			ELEMENT_TYPE ELEMENT_TYPE */
#define insertDoubleListElement( listHeadReference, insertPoint, newElement, ELEMENT_TYPE ) \
		{ \
		DATAPTR _listHead = *( listHeadReference ); \
		ELEMENT_TYPE *_listHeadPtr = DATAPTR_GET( _listHead ); \
		\
		/* Make sure that the elements being added are consistent */ \
		REQUIRES( insertPoint != newElement ); \
		REQUIRES( DATAPTR_GET( ( newElement )->prev ) == NULL && \
				  DATAPTR_GET( ( newElement )->next ) == NULL ); \
		\
		if( _listHeadPtr == NULL ) \
			{ \
			/* Further consistency check */ \
			REQUIRES( ( insertPoint ) == NULL ); \
			\
			/* It's an empty list, make this the new list */ \
			DATAPTR_SET_PTR( ( listHeadReference ), ( newElement ) ); \
			} \
		else \
			{ \
			if( ( insertPoint ) == NULL ) \
				{ \
				/* We're inserting at the start of the list, make this the \
				   new first element */ \
				DATAPTR_SET( ( newElement )->next, _listHeadPtr ); \
				DATAPTR_SET( _listHeadPtr->prev, ( newElement ) ); \
				DATAPTR_SET_PTR( ( listHeadReference ), ( newElement ) ); \
				} \
			else \
				{ \
				ELEMENT_TYPE *insertPointNext = DATAPTR_GET( ( insertPoint )->next ); \
				\
				/* Make sure that the links are consistent */ \
				ENSURES( insertPointNext == NULL || \
						 DATAPTR_GET( insertPointNext->prev ) == ( insertPoint ) ); \
				\
				/* Insert the element in the middle or the end of the list */ \
				DATAPTR_SET( ( newElement )->next, insertPointNext ); \
				DATAPTR_SET( ( newElement )->prev, ( insertPoint ) ); \
				\
				/* Update the links for the next and previous elements */ \
				if( insertPointNext != NULL ) \
					DATAPTR_SET( insertPointNext->prev, ( newElement ) ); \
				DATAPTR_SET( ( insertPoint )->next, ( newElement ) ); \
				} \
			} \
		}

/*								  DATAPTR			 ELEMENT_TYPE ELEMENT_TYPE */
#define insertDoubleListElements( listHeadReference, insertPoint, newStartElement, newEndElement, ELEMENT_TYPE ) \
		{ \
		DATAPTR _listHead = *( listHeadReference ); \
		ELEMENT_TYPE *_listHeadPtr = DATAPTR_GET( _listHead ); \
		\
		/* Make sure that the elements being added are consistent */ \
		REQUIRES( newStartElement != NULL && newEndElement != NULL ); \
		REQUIRES( insertPoint != newStartElement && insertPoint != newEndElement ); \
		REQUIRES( DATAPTR_GET( ( newStartElement )->prev ) == NULL && \
				  DATAPTR_GET( ( newEndElement )->next ) == NULL ); \
		\
		if( _listHeadPtr == NULL ) \
			{ \
			/* Further consistency check */ \
			REQUIRES( ( insertPoint ) == NULL ); \
			\
			/* It's an empty list, make this the new list */ \
			DATAPTR_SET_PTR( ( listHeadReference ), ( newStartElement ) ); \
			} \
		else \
			{ \
			if( ( insertPoint ) == NULL ) \
				{ \
				/* We're inserting at the start of the list, make this the \
				   new first element */ \
				DATAPTR_SET( ( newEndElement )->next, _listHeadPtr ); \
				DATAPTR_SET( _listHeadPtr->prev, ( newEndElement ) ); \
				DATAPTR_SET_PTR( ( listHeadReference ), ( newStartElement ) ); \
				} \
			else \
				{ \
				ELEMENT_TYPE *insertPointNext = DATAPTR_GET( ( insertPoint )->next ); \
				\
				/* Make sure that the links are consistent */ \
				ENSURES( insertPointNext == NULL || \
						 DATAPTR_GET( insertPointNext->prev ) == ( insertPoint ) ); \
				\
				/* Insert the element in the middle or the end of the list */ \
				DATAPTR_SET( ( newEndElement )->next, insertPointNext ); \
				DATAPTR_SET( ( newStartElement )->prev, ( insertPoint ) ); \
				\
				/* Update the links for the next and previous elements */ \
				if( insertPointNext != NULL ) \
					DATAPTR_SET( insertPointNext->prev, ( newEndElement ) ); \
				DATAPTR_SET( ( insertPoint )->next, ( newStartElement ) ); \
				} \
			} \
		}

/*								 DATAPTR			ELEMENT_TYPE */
#define deleteDoubleListElement( listHeadReference, element, ELEMENT_TYPE ) \
		{ \
		DATAPTR _listHead = *( listHeadReference ); \
		ELEMENT_TYPE *_listHeadPtr = DATAPTR_GET( _listHead ); \
		ELEMENT_TYPE *_elementPrev, *_elementNext; \
		\
		/* Make sure that the preconditions for safe delection are met */ \
		REQUIRES( _listHeadPtr != NULL && element != NULL ); \
		\
		_elementPrev = DATAPTR_GET( element->prev ); \
		_elementNext = DATAPTR_GET( element->next ); \
		\
		/* Make sure that the links are consistent */ \
		REQUIRES( _elementNext == NULL || \
				  DATAPTR_GET( _elementNext->prev ) == ( element ) ); \
		REQUIRES( _elementPrev == NULL || \
				  DATAPTR_GET( _elementPrev->next ) == ( element ) ); \
		\
		/* Unlink the element from the list */ \
		if( element == _listHeadPtr ) \
			{ \
			/* Further consistency check */ \
			REQUIRES( _elementPrev == NULL ); \
			\
			/* Special case for the first item */ \
			DATAPTR_SET_PTR( ( listHeadReference ), _elementNext ); \
			} \
		else \
			{ \
			/* Further consistency check */ \
			REQUIRES( _elementPrev != NULL ); \
			\
			/* Delete from the middle or the end of the list */ \
			DATAPTR_SET( _elementPrev->next, _elementNext ); \
			} \
		if( _elementNext != NULL ) \
			DATAPTR_SET( _elementNext->prev, _elementPrev ); \
		DATAPTR_SET( element->prev, NULL ); \
		DATAPTR_SET( element->next, NULL ); \
		}

#if 0

#define insertDoubleListElement( listHead, insertPoint, newElement, ELEMENT_TYPE ) \
		{ \
		ELEMENT_TYPE *_listHeadPtr = DATAPTR_GET( listHead ); \
		\
		/* Make sure that the elements being added are consistent */ \
		REQUIRES( insertPoint != newElement ); \
		REQUIRES( DATAPTR_GET( ( newElement )->prev ) == NULL && \
				  DATAPTR_GET( ( newElement )->next ) == NULL ); \
		\
		if( _listHeadPtr == NULL ) \
			{ \
			/* Further consistency check */ \
			REQUIRES( ( insertPoint ) == NULL ); \
			\
			/* If it's an empty list, make this the new list */ \
			DATAPTR_SET( listHead, ( newElement ) ); \
			} \
		else \
			{ \
			if( ( insertPoint ) == NULL ) \
				{ \
				/* We're inserting at the start of the list, make this the \
				   new first element */ \
				DATAPTR_SET( ( newElement )->next, _listHeadPtr ); \
				DATAPTR_SET( _listHeadPtr->prev, ( newElement ) ); \
				DATAPTR_SET( listHead, ( newElement ) ); \
				} \
			else \
				{ \
				ELEMENT_TYPE *insertPointNext = DATAPTR_GET( ( insertPoint )->next ); \
				\
				/* Make sure that the links are consistent */ \
				ENSURES( insertPointNext == NULL || \
						 DATAPTR_GET( insertPointNext->prev ) == ( insertPoint ) ); \
				\
				/* Insert the element in the middle or the end of the list */ \
				DATAPTR_SET( ( newElement )->next, insertPointNext ); \
				DATAPTR_SET( ( newElement )->prev, ( insertPoint ) ); \
				\
				/* Update the links for the next and previous elements */ \
				if( insertPointNext != NULL ) \
					DATAPTR_SET( insertPointNext->prev, ( newElement ) ); \
				DATAPTR_SET( ( insertPoint )->next, ( newElement ) ); \
				} \
			} \
		}

#define insertDoubleListElements( listHead, insertPoint, newStartElement, newEndElement, ELEMENT_TYPE ) \
		{ \
		ELEMENT_TYPE *_listHeadPtr = DATAPTR_GET( listHead ); \
		\
		/* Make sure that the elements being added are consistent */ \
		REQUIRES( newStartElement != NULL && newEndElement != NULL ); \
		REQUIRES( insertPoint != newStartElement && insertPoint != newEndElement ); \
		REQUIRES( DATAPTR_GET( ( newStartElement )->prev ) == NULL && \
				  DATAPTR_GET( ( newEndElement )->next ) == NULL ); \
		\
		if( _listHeadPtr == NULL ) \
			{ \
			/* Further consistency check */ \
			REQUIRES( ( insertPoint ) == NULL ); \
			\
			/* If it's an empty list, make this the new list */ \
			DATAPTR_SET( listHead, ( newStartElement ) ); \
			} \
		else \
			{ \
			if( ( insertPoint ) == NULL ) \
				{ \
				/* We're inserting at the start of the list, make this the \
				   new first element */ \
				DATAPTR_SET( ( newEndElement )->next, _listHeadPtr ); \
				DATAPTR_SET( _listHeadPtr->prev, ( newEndElement ) ); \
				DATAPTR_SET( listHead, ( newStartElement ) ); \
				} \
			else \
				{ \
				ELEMENT_TYPE *insertPointNext = DATAPTR_GET( ( insertPoint )->next ); \
				\
				/* Make sure that the links are consistent */ \
				ENSURES( insertPointNext == NULL || \
						 DATAPTR_GET( insertPointNext->prev ) == ( insertPoint ) ); \
				\
				/* Insert the element in the middle or the end of the list */ \
				DATAPTR_SET( ( newEndElement )->next, insertPointNext ); \
				DATAPTR_SET( ( newStartElement )->prev, ( insertPoint ) ); \
				\
				/* Update the links for the next and previous elements */ \
				if( insertPointNext != NULL ) \
					DATAPTR_SET( insertPointNext->prev, ( newEndElement ) ); \
				DATAPTR_SET( ( insertPoint )->next, ( newStartElement ) ); \
				} \
			} \
		}

#define deleteDoubleListElement( listHead, element, ELEMENT_TYPE ) \
		{ \
		ELEMENT_TYPE *_listHeadPtr = DATAPTR_GET( listHead ); \
		ELEMENT_TYPE *_elementPrev, *_elementNext; \
		\
		/* Make sure that the preconditions for safe delection are met */ \
		REQUIRES( _listHeadPtr != NULL && element != NULL ); \
		\
		_elementPrev = DATAPTR_GET( element->prev ); \
		_elementNext = DATAPTR_GET( element->next ); \
		\
		/* Make sure that the links are consistent */ \
		REQUIRES( _elementNext == NULL || \
				  DATAPTR_GET( _elementNext->prev ) == ( element ) ); \
		REQUIRES( _elementPrev == NULL || \
				  DATAPTR_GET( _elementPrev->next ) == ( element ) ); \
		\
		/* Unlink the element from the list */ \
		if( element == _listHeadPtr ) \
			{ \
			/* Further consistency check */ \
			REQUIRES( _elementPrev == NULL ); \
			\
			/* Special case for the first item */ \
			DATAPTR_SET( listHead, _elementNext ); \
			} \
		else \
			{ \
			/* Further consistency check */ \
			REQUIRES( _elementPrev != NULL ); \
			\
			/* Delete from the middle or the end of the list */ \
			DATAPTR_SET( _elementPrev->next, _elementNext ); \
			} \
		if( _elementNext != NULL ) \
			DATAPTR_SET( _elementNext->prev, _elementPrev ); \
		DATAPTR_SET( element->prev, NULL ); \
		DATAPTR_SET( element->next, NULL ); \
		}
#endif /* 0 */

#endif /* _LIST_DEFINED */
