/*------------------------------------------------------------------------**
** Version 1.0                                                            **
**                                                                        **
** Copyright (c) 2001 Frank Reid. All rights reserved.                    **
**                                                                        **
** Redistribution and use in source and binary forms, with or without     **
** modification, are permitted provided that the following conditions     **
** are met:                                                               **
**                                                                        **
** 1. Redistributions of source code must retain the above copyright      **
** notice, this list of conditions and the following disclaimer.          **
**                                                                        **
** 2. Redistributions in binary form must reproduce the above copyright   **
** notice, this list of conditions and the following disclaimer in the    **
** documentation and/or other materials provided with the distribution.   **
**                                                                        **
** 3. The end-user documentation included with the redistribution, if     **
** any, must include the following acknowledgment:                        **
**                                                                        **
** "This product includes software developed by the Frank Reid            **
** (http://filenet.wwiv.net/)."                                           **
**                                                                        **
** Alternately, this acknowledgment may appear in the software itself,    **
** if and wherever such third-party acknowledgments normally appear.      **
**                                                                        **
** 4. The name "Frank Reid" must not be used to endorse or promote        **
** products derived from this software without prior written              **
** permission. For written permission, please contact                     **
** u1@n1160.filenet.wwiv.net.                                             **
**                                                                        **
** 5. Products derived from this software may not be called               **
** "PPP Project", nor may "PPP Project" appear in their name,             **
** without prior written permission of Frank Reid.                        **
**                                                                        **
** THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED         **
** WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES      **
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE            **
** DISCLAIMED.  IN NO EVENT SHALL THE FRANK REID OR ITS CONTRIBUTORS      **
** BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,    **
** OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT   **
** OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR     **
** BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,  **
** WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE   **
** OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,      **
** EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                     **
**------------------------------------------------------------------------*/
#define UNPACK_NO_MEM  -10000
#define UNPACK_BAD_IN  -10001
#define UNPACK_BAD_OUT -10002

/*
    UU.H        --      UU encode/decode library

    Copyright (C) 1992  Walter Andrew Nolan

                        wan@eng.ufl.edu

                        ECS-MIS
                        College of Engineering
                        University of Florida

                        Version 1.00



    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    */

#define UU_SUCCESS   0
#define UU_NO_MEM    -10000
#define UU_BAD_BEGIN -10001
#define UU_CANT_OPEN -10002
#define UU_CHECKSUM  -10003
#define UU_BAD_END   -10004

#define PACK_BAD_IN  -10000
#define PACK_BAD_OUT -10001


long uuencode (FILE *in, FILE *out, char *name);

int uudecode (FILE *in, char *path, char*fname);

int uu_unpack (char * inname, char * outname, char *outpath);


