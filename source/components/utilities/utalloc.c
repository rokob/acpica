/******************************************************************************
 * 
 * Module Name: cmalloc - local memory allocation routines
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, Intel Corp.  All rights
 * reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code.  No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions 
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.  
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee 
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.  
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE. 
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************/

#define __CMALLOC_C__

#include <acpi.h>
#include <acpiobj.h>
#include <interpreter.h>
#include <namespace.h>
#include <globals.h>

#define _COMPONENT          MISCELLANEOUS
        MODULE_NAME         ("cmalloc");


/*
 * Most of this code is for tracking memory leaks in the subsystem, and it
 * gets compiled out when the ACPI_DEBUG flag is not set.  It works like so:
 * every memory allocation is kept track of in a doubly linked list.  Each
 * element contains the caller's component, module name, function name, and
 * line number.  _CmAllocate and _CmCallocate call CmAddElementToAllocList
 * to add an element to the list; deletion occurs in the bosy of _CmFree.
 */

#ifdef ACPI_DEBUG

/*****************************************************************************
 * 
 * FUNCTION:    CmSearchAllocList
 *
 * PARAMETERS:  Address             - Address of allocated memory
 *
 * RETURN:      A list element if found; NULL otherwise.
 *
 * DESCRIPTION: Inserts an element into the global allocation tracking list.
 *
 ****************************************************************************/

ALLOCATION_INFO *
CmSearchAllocList (
    void                    *Address)
{
    ALLOCATION_INFO         *Element = Gbl_HeadAllocPtr;


    /* Search for the address. note - this always searches the entire list...*/

    for (;;)
    {
        if (Element == NULL)
        {
            return NULL;
        }
        
        if (Element->Address == Address)
        {
            return Element;
        }

        Element = Element->Next; 
    }
}


/*****************************************************************************
 * 
 * FUNCTION:    CmAddElementToAllocList
 *
 * PARAMETERS:  Address             - Address of allocated memory
 *              Size                - Size of the allocation
 *              AllocType           - MEM_MALLOC or MEM_CALLOC
 *              Component           - Component type of caller
 *              Module              - Source file name of caller
 *              Line                - Line number of caller
 *              Function            - Calling function name
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Inserts an element into the global allocation tracking list.
 *
 ****************************************************************************/

void
CmAddElementToAllocList (
    void                    *Address,
    UINT32                  Size,
    UINT8                   AllocType,
    UINT32                  Component,
    ACPI_STRING             Module,
    INT32                   Line)
{
    ALLOCATION_INFO         *Element;


    FUNCTION_TRACE ("CmAddElementToAllocList");
    

    CmAcquireMutex (MTX_MEMORY);
    
    /* Keep track of the running total of all allocations. */
    Gbl_CurrentAllocCount++;
    Gbl_RunningAllocCount++;
    if (Gbl_MaxConcurrentAllocCount < Gbl_CurrentAllocCount)
    {
    	Gbl_MaxConcurrentAllocCount = Gbl_CurrentAllocCount;
    }
    
    Gbl_CurrentAllocSize += Size;
    Gbl_RunningAllocSize += Size;
    if (Gbl_MaxConcurrentAllocSize < Gbl_CurrentAllocSize)
    {
    	Gbl_MaxConcurrentAllocSize = Gbl_CurrentAllocSize;
    }
    
    /* If the head pointer is null, create the first element and fill it in. */

    if (NULL == Gbl_HeadAllocPtr)
    {
        Gbl_HeadAllocPtr = (ALLOCATION_INFO *) OsdCallocate (sizeof (ALLOCATION_INFO));
        
        /* error check */
        
        Gbl_TailAllocPtr = Gbl_HeadAllocPtr;
    }

    else
    {
        Gbl_TailAllocPtr->Next = (ALLOCATION_INFO *) OsdCallocate (sizeof (ALLOCATION_INFO));
        
        /* error check */
        
        Gbl_TailAllocPtr->Next->Previous = Gbl_TailAllocPtr;
        Gbl_TailAllocPtr = Gbl_TailAllocPtr->Next;
    }

    /* 
     * Search list for this address to make sure it is not already on the list.
     * This will catch several kinds of problems.
     */

    Element = CmSearchAllocList (Address);
    if (Element)
    {
        REPORT_ERROR ("CmAddElementToAllocList: Address already present in list!");
        DEBUG_PRINT (ACPI_ERROR, ("Element %p Address %p\n", Element, Address));

        BREAKPOINT3;
    }

    /* Fill in the instance data. */    
    
    Gbl_TailAllocPtr->Address   = Address;
    Gbl_TailAllocPtr->Size      = Size;
    Gbl_TailAllocPtr->AllocType = AllocType;
    Gbl_TailAllocPtr->Component = Component;
    Gbl_TailAllocPtr->Line      = Line;
    STRCPY (Gbl_TailAllocPtr->Module, Module);
    
    CmReleaseMutex (MTX_MEMORY);
    return_VOID;
}


/*****************************************************************************
 * 
 * FUNCTION:    CmDeleteElementFromAllocList
 *
 * PARAMETERS:  Address             - Address of allocated memory
 *              Component           - Component type of caller
 *              Module              - Source file name of caller
 *              Line                - Line number of caller
 *
 * RETURN:      
 *
 * DESCRIPTION: Inserts an element into the global allocation tracking list.
 *
 ****************************************************************************/

void
CmDeleteElementFromAllocList (
    void                    *Address,
    UINT32                  Component,
    ACPI_STRING             Module,
    INT32                   Line)
{
    ALLOCATION_INFO         *Element;
    UINT32					Size;
    UINT32                  DwordLen;
    UINT32                  i;
    

    FUNCTION_TRACE ("CmDeleteElementFromAllocList");

    /* cases: none, one, multiple. */

    if (NULL == Gbl_HeadAllocPtr)
    {
        /* Boy we got problems. */

        _REPORT_ERROR (Module, Line, Component,
            "CmDeleteElementFromAllocList: Empty allocation list and someone's calling CmFree.");
        
        return_VOID;
    }

    
    CmAcquireMutex (MTX_MEMORY);
    
    /* Keep track of the amount of memory allocated. */
    Size = 0;
    Gbl_CurrentAllocCount--;

    if (Gbl_HeadAllocPtr == Gbl_TailAllocPtr)
    {   
        if (Address != Gbl_HeadAllocPtr->Address)
        {
            _REPORT_ERROR (Module, Line, Component,
                "CmDeleteElementFromAllocList: Deleting non-allocated memory...");

            goto Cleanup;
        }
        
        Size = Gbl_HeadAllocPtr->Size;
        
        OsdFree (Gbl_HeadAllocPtr);
        Gbl_HeadAllocPtr = NULL;
        Gbl_TailAllocPtr = NULL;
        
        DEBUG_PRINT (TRACE_ALLOCATIONS,
            ("_CmFree: Allocation list deleted.  No more outstanding allocations.\n"));
    
        goto Cleanup;
    }
    

    /* Search list for this address */

    Element = CmSearchAllocList (Address);
    if (Element)
    {
        /* cases: head, tail, other */

        if (Element == Gbl_HeadAllocPtr)
        {
            Element->Next->Previous = NULL;
            Gbl_HeadAllocPtr = Element->Next;
        }

        else
        {
            if (Element == Gbl_TailAllocPtr)
            {
                Element->Previous->Next = NULL;
                Gbl_TailAllocPtr = Element->Previous;
            }

            else
            {
                Element->Previous->Next = Element->Next;
                Element->Next->Previous = Element->Previous;
            }
        }       
        
        /* Mark the segment as deleted */

        if (Element->Size > sizeof (void *))
        {
            DwordLen = Element->Size / 4;

            for (i = 0; i < DwordLen; i++)
            {
                ((void **) Element->Address)[i] = (void *) 0x00DEAD00;
            }
        }

        Size = Element->Size;
        OsdFree (Element);
    }
            
    else
    {
        _REPORT_ERROR (Module, Line, Component,
            "_CmFree: Entry not found in list");
        DEBUG_PRINT (TRACE_ALLOCATIONS,
            ("_CmFree: Entry %p was not found in allocation list\n", Address));
    
    }


Cleanup:
    
    Gbl_CurrentAllocSize -= Size;
    CmReleaseMutex (MTX_MEMORY);

    return_VOID;
}


/*****************************************************************************
 * 
 * FUNCTION:    CmDumpAllocationInfo
 *
 * PARAMETERS:  
 *
 * RETURN:      None  
 *
 * DESCRIPTION: Print some info about the outstanding allocations.
 *
 ****************************************************************************/

void
CmDumpAllocationInfo (
	void)
{
    FUNCTION_TRACE ("CmDumpAllocationInfo");
    
    DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
    	("Current outstanding allocations: %d (%d b / %d Kb)\n",
    	Gbl_CurrentAllocCount, Gbl_CurrentAllocSize, Gbl_CurrentAllocSize / 1024));
    DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
		("Maximum concurrent allocations thus far: %d (%d b / %d Kb)\n",
		Gbl_MaxConcurrentAllocCount, Gbl_MaxConcurrentAllocSize, Gbl_MaxConcurrentAllocSize / 1024));
	DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
    	("Current number of allocated internal objects: %d (%d b / %d Kb)\n",
    	Gbl_CurrentObjectCount, Gbl_CurrentObjectSize, Gbl_CurrentObjectSize / 1024));
    DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
    	("Maximum concurrent number of allocated internal objects: %d (%d b / %d Kb)\n",
    	Gbl_MaxConcurrentObjectCount, Gbl_MaxConcurrentObjectSize, Gbl_MaxConcurrentObjectSize / 1024));
    DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
    	("Total number of allocated internal objects: %d (%d b / %d Kb)\n",
    	Gbl_RunningObjectCount, Gbl_RunningObjectSize, Gbl_RunningObjectSize / 1024));
    DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
    	("Total number of allocations: %d (%d b / %d Kb)\n",
    	Gbl_RunningAllocCount, Gbl_RunningAllocSize, Gbl_RunningAllocSize / 1024));

	return_VOID;
}


/*****************************************************************************
 * 
 * FUNCTION:    CmDumpCurrentAllocations
 *
 * PARAMETERS:  Component           - Component(s) to dump info for.
 *              Module              - Module to dump info for.  NULL means all.
 *
 * RETURN:      None  
 *
 * DESCRIPTION: Print a list of all outstanding allocations.
 *
 ****************************************************************************/

void
CmDumpCurrentAllocations (
    UINT32                  Component,
    ACPI_STRING             Module)
{
    ALLOCATION_INFO         *Element = Gbl_HeadAllocPtr;
    UINT32                  i;
    

    FUNCTION_TRACE ("CmDumpCurrentAllocations");
    

    if (Element == NULL)
    {
        DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES, ("No outstanding allocations.\n"));
        return_VOID;
    }


    /*
     * Walk the allocation list.
     */

    CmAcquireMutex (MTX_MEMORY);

    DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES, ("Outstanding allocations:\n"));

    for (i = 1; ; i++)  /* Just a counter */
    {
        if ((Element->Component & Component) &&
            ((Module == NULL) || (0 == STRCMP (Module, Element->Module))))
        {
            DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                ("%p: Length %04x %10.10s Line %d",
                Element->Address, Element->Size, Element->Module, Element->Line));

            /* Most of the elements will be internal objects. */

            if (Element->Size >= sizeof (ACPI_OBJECT_Common))
            {
                if (((ACPI_OBJECT_INTERNAL *)(Element->Address))->Common.DataType == DESC_TYPE_ACPI_OBJ)
                {
                    DEBUG_PRINT_RAW (TRACE_ALLOCATIONS | TRACE_TABLES, (" Type %s", 
                        Gbl_NsTypeNames[((ACPI_OBJECT_INTERNAL *)(Element->Address))->Common.Type]));
                }
            }

            DEBUG_PRINT_RAW (TRACE_ALLOCATIONS | TRACE_TABLES, ("\n")); 
        }
        
        if (Element->Next == NULL)
        {
            break;
        }
        
        Element = Element->Next;
    }

    CmReleaseMutex (MTX_MEMORY);

    DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
        ("Total number of unfreed allocations = %d\n", i));
            
    DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
        ("Stack Ptrs: Obj=%d Mth=%d\n",
            AmlObjStackLevel(), AmlMthStackLevel()));
 
    return_VOID;
}   

#endif  /* Debug routines for memory leak detection */


/*****************************************************************************
 * 
 * FUNCTION:    _CmAllocate
 *
 * PARAMETERS:  Size                - Size of the allocation
 *              Component           - Component type of caller
 *              Module              - Source file name of caller
 *              Line                - Line number of caller
 *
 * RETURN:      Address of the allocated memory on success, NULL on failure.
 *
 * DESCRIPTION: The subsystem's equivalent of malloc.
 *
 ****************************************************************************/

void *
_CmAllocate (
    UINT32                  Size,
    UINT32                  Component,
    ACPI_STRING             Module,
    INT32                   Line)
{
    void                    *Address = NULL;


    FUNCTION_TRACE_U32 ("_CmAllocate", Size);
    

    Address = OsdAllocate (Size);
    if (!Address)
    {
        /* Report allocation error */

        _REPORT_ERROR (Module, Line, Component,
            "CmAllocate: Memory allocation failure");

        DEBUG_PRINT (ACPI_ERROR, ("CmAllocate: Could not allocate size 0x%x\n", Size));

        return_VALUE (NULL);
    }
    
    CmAddElementToAllocList (Address, Size, MEM_MALLOC, Component, Module, Line);

    DEBUG_PRINT (TRACE_ALLOCATIONS, ("CmAllocate: %x Size 0x%x\n",
        Address, Size));

    return_VALUE (Address);
}


/*****************************************************************************
 * 
 * FUNCTION:    _CmCallocate
 *
 * PARAMETERS:  Size                - Size of the allocation
 *              Component           - Component type of caller
 *              Module              - Source file name of caller
 *              Line                - Line number of caller
 *
 * RETURN:      Address of the allocated memory on success, NULL on failure.
 *
 * DESCRIPTION: Subsystem equivalent of calloc.
 *
 ****************************************************************************/

void *
_CmCallocate (
    UINT32                  Size,
    UINT32                  Component,
    ACPI_STRING             Module,
    INT32                   Line)
{
    void                    *Address = NULL;


    FUNCTION_TRACE_U32 ("_CmCallocate", Size);
       
    
    Address = OsdCallocate (Size);
    
    if (!Address)
    {
        /* Report allocation error */

        _REPORT_ERROR (Module, Line, Component,
            "CmCallocate: Memory allocation failure");

        DEBUG_PRINT (ACPI_ERROR, ("CmCallocate: Could not allocate size 0x%x\n", Size));

        return_VALUE (NULL);
    }

    CmAddElementToAllocList (Address, Size, MEM_CALLOC, Component, Module, Line);
    
    DEBUG_PRINT (TRACE_ALLOCATIONS, ("CmCallocate: %x Size 0x%x\n",
        Address, Size));

    return_VALUE (Address);
}


/*****************************************************************************
 * 
 * FUNCTION:    _CmFree
 *
 * PARAMETERS:  Address             - Address of the memory to deallocate
 *              Component           - Component type of caller
 *              Module              - Source file name of caller
 *              Line                - Line number of caller
 *
 * RETURN:      None
 *
 * DESCRIPTION: Frees the memory at Address
 *
 ****************************************************************************/

void
_CmFree (
    void                    *Address,
    UINT32                  Component,
    ACPI_STRING             Module,
    INT32                   Line)
{
    FUNCTION_TRACE_PTR ("_CmFree", Address);
    

    if (NULL == Address)
    {
        _REPORT_ERROR (Module, Line, Component,
            "_CmFree: Trying to delete a NULL address.");

        return_VOID;
    }   
    
    CmDeleteElementFromAllocList (Address, Component, Module, Line);
    OsdFree (Address);
    
    DEBUG_PRINT (TRACE_ALLOCATIONS, ("CmFree: %x freed\n", Address));

    return_VOID;
}


/*****************************************************************************
 * 
 * FUNCTION:    _CmAllocateObjectDesc
 *
 * PARAMETERS:  ModuleName          - Caller's module name (for error output)
 *              LineNumber          - Caller's line number (for error output)
 *              ComponentId         - Caller's component ID (for error output)
 *              Message             - Error message to use on failure
 *
 * RETURN:      Pointer to newly allocated object descriptor.  Null on error
 *
 * DESCRIPTION: Allocate a new object descriptor.  Gracefully handle
 *              error conditions.
 *
 ****************************************************************************/

void *
_CmAllocateObjectDesc (
    char                    *ModuleName, 
    INT32                   LineNumber, 
    INT32                   ComponentId)
{
    ACPI_OBJECT_INTERNAL    *NewDesc;
    

    FUNCTION_TRACE ("_AllocateObjectDesc");


    /* Attempt to allocate new descriptor */

    NewDesc = _CmCallocate (sizeof (ACPI_OBJECT_INTERNAL), ComponentId,
                                ModuleName, LineNumber);
        
    if (!NewDesc)
    {
        /* Allocation failed */
        
        _REPORT_ERROR (ModuleName, LineNumber, ComponentId, 
                        "Could not allocate Object Descriptor");
    }

    else
    {
        /* Mark the descriptor type */

        NewDesc->Common.DataType = DESC_TYPE_ACPI_OBJ;

        DEBUG_PRINT (TRACE_ALLOCATIONS, ("AllocateObjectDesc: %x Size 0x%x\n",
                        NewDesc, sizeof (ACPI_OBJECT_INTERNAL)));
    }

    return_VALUE (NewDesc);
}


/*****************************************************************************
 * 
 * FUNCTION:    CmInitStaticObject
 *
 * PARAMETERS:  ObjDesc             - Pointer to a "static" object - on stack
 *                                    or in the data segment.
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Initialize a static object.  Sets flags to disallow dynamic
 *              deletion of the object.
 *
 ****************************************************************************/

void
CmInitStaticObject (
    ACPI_OBJECT_INTERNAL    *ObjDesc)
{

    FUNCTION_TRACE_PTR ("CmInitStaticObject", ObjDesc);


    if (!ObjDesc)
    {
        return_VOID;
    }


    /*
     * Clear the entire descriptor 
     */
    MEMSET ((void *) ObjDesc, 0, sizeof (ACPI_OBJECT_INTERNAL));


    /*
     * Initialize the header fields
     * 1) This is an ACPI_OBJECT_INTERNAL descriptor
     * 2) The size is the full object (worst case)
     * 3) The flags field indicates static allocation 
     */

    ObjDesc->Common.DataType = DESC_TYPE_ACPI_OBJ;
    ObjDesc->Common.Size = sizeof (ACPI_OBJECT_INTERNAL);
    ObjDesc->Common.Flags = AO_STATIC_ALLOCATION;

    return_VOID;
}

