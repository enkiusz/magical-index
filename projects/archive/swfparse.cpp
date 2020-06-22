// SWF file parser.
//
//////////////////////////////////////////////////////////////////////


//!!@ dump sound data

#include <stdio.h>
#include <string.h>


#if defined(_DEBUG) && defined(_WIN32)
	#include <windows.h>
    #define DEBUG_ASSERT    DebugBreak()
#else
    #define DEBUG_ASSERT
	typedef unsigned long BOOL;
#endif

// Global Types
typedef unsigned long U32, *P_U32, **PP_U32;
typedef signed long S32, *P_S32, **PP_S32;
typedef unsigned short U16, *P_U16, **PP_U16;
typedef signed short S16, *P_S16, **PP_S16;
typedef unsigned char U8, *P_U8, **PP_U8;
typedef signed char S8, *P_S8, **PP_S8;
typedef signed long SFIXED, *P_SFIXED;
typedef signed long SCOORD, *P_SCOORD;

typedef struct SPOINT
{
    SCOORD x;
    SCOORD y;
} SPOINT, *P_SPOINT;

typedef struct SRECT 
{
    SCOORD xmin;
    SCOORD xmax;
    SCOORD ymin;
    SCOORD ymax;
} SRECT, *P_SRECT;

// Start Sound Flags
enum {
	soundHasInPoint		= 0x01,
	soundHasOutPoint	= 0x02,
	soundHasLoops		= 0x04,
	soundHasEnvelope	= 0x08

	// the upper 4 bits are reserved for synchronization flags
};

enum {
	fillGradient		=	0x10,
	fillLinearGradient 	=	0x10,
	fillRadialGradient 	=	0x12,
	fillMaxGradientColors 	=	8,
	// Texture/bitmap fills
	fillBits			=	0x40	// if this bit is set, must be a bitmap pattern
};

// Flags for defining a shape character
enum {
		// These flag codes are used for state changes - and as return values from ShapeParser::GetEdge()
		eflagsMoveTo	   = 0x01,
		eflagsFill0	   	   = 0x02,
		eflagsFill1		   = 0x04,
		eflagsLine		   = 0x08,
		eflagsNewStyles	   = 0x10,

		eflagsEnd 	   	   = 0x80  // a state change with no change marks the end
};

typedef struct MATRIX
{
    SFIXED a;
    SFIXED b;
    SFIXED c;
    SFIXED d;
    SCOORD tx;
    SCOORD ty;
} MATRIX, *P_MATRIX;

typedef struct CXFORM
{
    S32 flags;
    enum
    { 
        needA=0x1,	// Set if we need the multiply terms.
        needB=0x2	// Set if we need the constant terms.
    };
    S16 aa, ab;	// a is multiply factor, b is addition factor
    S16 ra, rb;
    S16 ga, gb;
    S16 ba, bb;
} CXFORM, *P_CXFORM;

#ifndef NULL
#define NULL 0
#endif

// Tag values that represent actions or data in a Flash script.
enum
{ 
    stagEnd 				= 0,
    stagShowFrame 			= 1,
    stagDefineShape		 	= 2,
    stagFreeCharacter 		= 3,
    stagPlaceObject 		= 4,
    stagRemoveObject 		= 5,
    stagDefineBits 			= 6,
    stagDefineButton 		= 7,
    stagJPEGTables 			= 8,
    stagSetBackgroundColor	= 9,
    stagDefineFont			= 10,
    stagDefineText			= 11,
    stagDoAction			= 12,
    stagDefineFontInfo		= 13,
    stagDefineSound			= 14,	// Event sound tags.
    stagStartSound			= 15,
    stagDefineButtonSound	= 17,
    stagSoundStreamHead		= 18,
    stagSoundStreamBlock	= 19,
    stagDefineBitsLossless	= 20,	// A bitmap using lossless zlib compression.
    stagDefineBitsJPEG2		= 21,	// A bitmap using an internal JPEG compression table.
    stagDefineShape2		= 22,
    stagDefineButtonCxform	= 23,
    stagProtect				= 24,	// This file should not be importable for editing.

    // These are the new tags for Flash 3.
    stagPlaceObject2		= 26,	// The new style place w/ alpha color transform and name.
    stagRemoveObject2		= 28,	// A more compact remove object that omits the character tag (just depth).
    stagDefineShape3		= 32,	// A shape V3 includes alpha values.
    stagDefineText2			= 33,	// A text V2 includes alpha values.
    stagDefineButton2		= 34,	// A button V2 includes color transform, alpha and multiple actions
    stagDefineBitsJPEG3		= 35,	// A JPEG bitmap with alpha info.
    stagDefineBitsLossless2 = 36,	// A lossless bitmap with alpha info.
    stagDefineSprite		= 39,	// Define a sequence of tags that describe the behavior of a sprite.
    stagNameCharacter		= 40,	// Name a character definition, character id and a string, (used for buttons, bitmaps, sprites and sounds).
    stagFrameLabel			= 43,	// A string label for the current frame.
    stagSoundStreamHead2	= 45,	// For lossless streaming sound, should not have needed this...
    stagDefineMorphShape	= 46,	// A morph shape definition
    stagDefineFont2			= 48,	// 
};

// PlaceObject2 Flags
enum
{
    splaceMove			= 0x01,	// this place moves an exisiting object
    splaceCharacter		= 0x02,	// there is a character tag	(if no tag, must be a move)
    splaceMatrix		= 0x04,	// there is a matrix (matrix)
    splaceColorTransform= 0x08,	// there is a color transform (cxform with alpha)
    splaceRatio			= 0x10,	// there is a blend ratio (word)
    splaceName			= 0x20,	// there is an object name (string)
	splaceDefineClip	= 0x40  // this shape should open or close a clipping bracket (character != 0 to open, character == 0 to close)
    // one bit left for expansion
};

// Action codes
enum {
	sactionHasLength	= 0x80,
	sactionNone			= 0x00,
	sactionGotoFrame	= 0x81,	// frame num (WORD)
	sactionGetURL		= 0x83,	// url (STR), window (STR)
	sactionNextFrame	= 0x04,
	sactionPrevFrame	= 0x05,
	sactionPlay			= 0x06,
	sactionStop			= 0x07,
	sactionToggleQuality= 0x08,
	sactionStopSounds	= 0x09,
	sactionWaitForFrame	= 0x8A,	// frame needed (WORD), actions to skip (BYTE)
	sactionSetTarget	= 0x8B,	// name (STR)
	sactionGotoLabel	= 0x8C	// name (STR)
};

//////////////////////////////////////////////////////////////////////
// Input script object definition.
//////////////////////////////////////////////////////////////////////

// An input script object.  This object represents a script created from 
// an external file that is meant to be inserted into an output script.
struct CInputScript  
{
    // Pointer to file contents buffer.
    U8 *m_fileBuf;

    // File state information.
    U32 m_filePos;
    U32 m_fileSize;
    U32 m_fileStart;
    U16 m_fileVersion;

    // Bit Handling
    S32 m_bitPos;
    U32 m_bitBuf;

    // Tag parsing information.
    U32 m_tagStart;
    U32 m_tagEnd;
    U32 m_tagLen;
    
 	// Parsing information.
	S32 m_nFillBits;
	S32 m_nLineBits;

   // Set to true if we wish to dump all contents long form
    U32 m_dumpAll;

    // if set to true will dump image guts (i.e. jpeg, zlib, etc. data)
    U32 m_dumpGuts;
    
    // Handle to output file.
    FILE *m_outputFile;

    // Constructor/destructor.
    CInputScript();
    ~CInputScript();

    // Tag scanning methods.
    U16 GetTag(void);
    U8 GetByte(void);
    U16 GetWord(void);
    U32 GetDWord(void);
    void GetRect(SRECT *r);
    void GetMatrix(MATRIX *matrix);
    void GetCxform(CXFORM *cxform, BOOL hasAlpha);
    char *GetString(void);
	U32 GetColor(void);

    // Routines for reading arbitrary sized bit fields from the stream.
    // Always call start bits before gettings bits and do not intermix 
    // these calls with GetByte, etc...	
    void InitBits();
    S32 GetSBits(S32 n);
    U32 GetBits(S32 n);
	void where();

	// Tag subcomponent parsing methods
	// For shapes
    void ParseShapeStyle(char *str);
    BOOL ParseShapeRecord(char *str);
    void ParseButtonRecord(char *str, U32 byte);

    // Parsing methods.
    void ParseEnd(char *str);								// 00: stagEnd
    void ParseShowFrame(char *str, U32 frame, U32 offset);	// 01: stagShowFrame
    void ParseDefineShape(char *str);						// 02: stagDefineShape
    void ParseFreeCharacter(char *str);                     // 03: stagFreeCharacter
    void ParsePlaceObject(char *str);                       // 04: stagPlaceObject
    void ParseRemoveObject(char *str);                      // 05: stagRemoveObject
    void ParseDefineBits(char *str);                        // 06: stagDefineBits
    void ParseDefineButton(char *str);       //x 07: stagDefineButton
    void ParseJPEGTables(char *str);		                // 08: stagJPEGTables
    void ParseSetBackgroundColor(char *str);                // 09: stagSetBackgroundColor
    void ParseDefineFont(char *str);         //x 10: stagDefineFont
    void ParseDefineText(char *str);         //x 11: stagDefineText
    void ParseDoAction(char *str, bool fPrintTag=true);                          // 12: stagDoAction	
    void ParseDefineFontInfo(char *str);     //x 13: stagDefineFontInfo
    void ParseDefineSound(char *str);                       // 14: stagDefineSound
    void ParseStartSound(char *str);                        // 15: stagStartSound
    void ParseStopSound(char *str);                         // 16: stagStopSound
    void ParseDefineButtonSound(char *str);                 // 17: stagDefineButtonSound
    void ParseSoundStreamHead(char *str); 	                // 18: stagSoundStreamHead
    void ParseSoundStreamBlock(char *str);                  // 19: stagSoundStreamBlock
    void ParseDefineBitsLossless(char *str);                // 20: stagDefineBitsLossless
    void ParseDefineBitsJPEG2(char *str);                   // 21: stagDefineBitsJPEG2
    void ParseDefineShape2(char *str);       //x 22: stagDefineShape2
    void ParseDefineButtonCxform(char *str);	            // 23: stagDefineButtonCxform
    void ParseProtect(char *str);           	            // 24: stagProtect
    void ParsePlaceObject2(char *str);                      // 26: stagPlaceObject2
    void ParseRemoveObject2(char *str);                     // 28: stagRemoveObject2
    void ParseDefineShape3(char *str);       //x 32: stagDefineShape3
    void ParseDefineText2(char *str);        //x 33: stagDefineText2
    void ParseDefineButton2(char *str);      //x 34: stagDefineButton2
    void ParseDefineBitsJPEG3(char *str);                   // 35: stagDefineBitsJPEG3
    void ParseDefineBitsLossless2(char *str);               // 36: stagDefineBitsLossless2
    void ParseDefineMouseTarget(char *str);                 // 38: stagDefineMouseTarget
    void ParseDefineSprite(char *str);       //x 39: stagDefineSprite
    void ParseNameCharacter(char *str);                     // 40: stagNameCharacter
    void ParseFrameLabel(char *str);                        // 43: stagFrameLabel
    void ParseSoundStreamHead2(char *str); 	                // 45: stagSoundStreamHead2
    void ParseDefineMorphShape(char *str);   //x 46: stagDefineMorphShape
    void ParseDefineFont2(char *str);        //x 48: stagDefineFont2
    void ParseUnknown(char *str);
    void ParseTags(bool sprite, U32 tabs);
    BOOL ParseFile(char * pInput);
    void S_DumpImageGuts(char *str);
};

#define INDENT  printf("                 ");

//////////////////////////////////////////////////////////////////////
// Inline input script object methods.
//////////////////////////////////////////////////////////////////////

//
// Inlines to parse a Flash file.
//
inline U8 CInputScript::GetByte(void) 
{
	//printf("GetByte: filePos: %02x [%02x]\n", m_filePos, m_fileBuf[m_filePos]);
	InitBits();
    return m_fileBuf[m_filePos++];
}

inline U16 CInputScript::GetWord(void)
{
	//printf("GetWord: filePos: %02x\n", m_filePos);
    U8 * s = m_fileBuf + m_filePos;
    m_filePos += 2;
	InitBits();
    return (U16) s[0] | ((U16) s[1] << 8);
}

inline U32 CInputScript::GetDWord(void)
{
	//printf("GetDWord: filePos: %02x\n", m_filePos);
    U8 * s = m_fileBuf + m_filePos;
    m_filePos += 4;
	InitBits();
    return (U32) s[0] | ((U32) s[1] << 8) | ((U32) s[2] << 16) | ((U32) s [3] << 24);
}




//////////////////////////////////////////////////////////////////////
// Input script object methods.
//////////////////////////////////////////////////////////////////////

CInputScript::CInputScript(void)
// Class constructor.
{
    // Initialize the input pointer.
    m_fileBuf = NULL;

    // Initialize the file information.
    m_filePos = 0;
    m_fileSize = 0;
    m_fileStart = 0;
    m_fileVersion = 0;

    // Initialize the bit position and buffer.
    m_bitPos = 0;
    m_bitBuf = 0;

    // Initialize the output file.
    m_outputFile = NULL;

    // Set to true if we wish to dump all contents long form
    m_dumpAll = true;

    // if set to true will dump image guts (i.e. jpeg, zlib, etc. data)
    m_dumpGuts = true;

    return;
}


CInputScript::~CInputScript(void)
// Class destructor.
{
    // Free the buffer if it is there.
    if (m_fileBuf)
    {
        delete m_fileBuf;
        m_fileBuf = NULL;
        m_fileSize = 0;
    }
}


U16 CInputScript::GetTag(void)
{
    // Save the start of the tag.
    m_tagStart = m_filePos;
    
    // Get the combined code and length of the tag.
    U16 code = GetWord();

    // The length is encoded in the tag.
    U32 len = code & 0x3f;

    // Remove the length from the code.
    code = code >> 6;

    // Determine if another long word must be read to get the length.
    if (len == 0x3f) len = (U32) GetDWord();

    // Determine the end position of the tag.
    m_tagEnd = m_filePos + (U32) len;
    m_tagLen = (U32) len;

    return code;
}


void CInputScript::GetRect (SRECT * r)
{
    InitBits();
    int nBits = (int) GetBits(5);
    r->xmin = GetSBits(nBits);
    r->xmax = GetSBits(nBits);
    r->ymin = GetSBits(nBits);
    r->ymax = GetSBits(nBits);
}


void CInputScript::GetMatrix(MATRIX* mat)
{
    InitBits();

    // Scale terms
    if (GetBits(1))
    {
        int nBits = (int) GetBits(5);
        mat->a = GetSBits(nBits);
        mat->d = GetSBits(nBits);
    }
    else
    {
     	mat->a = mat->d = 0x00010000L;
    }

    // Rotate/skew terms
    if (GetBits(1))
    {
        int nBits = (int)GetBits(5);
        mat->b = GetSBits(nBits);
        mat->c = GetSBits(nBits);
    }
    else
    {
     	mat->b = mat->c = 0;
    }

    // Translate terms
    int nBits = (int) GetBits(5);
    mat->tx = GetSBits(nBits);
    mat->ty = GetSBits(nBits);
}


void CInputScript::GetCxform(CXFORM* cx, BOOL hasAlpha)
{
    InitBits();

    cx->flags = (int) GetBits(2);
    int nBits = (int) GetBits(4);

	//printf("GetCxform: flags: %d nBits:%d\n", cx->flags, nBits);

    cx->aa = 256; cx->ab = 0;
    if (cx->flags & CXFORM::needA)
    {
        cx->ra = (S16) GetSBits(nBits);
        cx->ga = (S16) GetSBits(nBits);
        cx->ba = (S16) GetSBits(nBits);
        if (hasAlpha) cx->aa = (S16) GetSBits(nBits);
    }
    else
    {
        cx->ra = cx->ga = cx->ba = 256;
    }
    if (cx->flags & CXFORM::needB)
    {
        cx->rb = (S16) GetSBits(nBits);
        cx->gb = (S16) GetSBits(nBits);
        cx->bb = (S16) GetSBits(nBits);
        if (hasAlpha) cx->ab = (S16) GetSBits(nBits);
    }
    else
    {
        cx->rb = cx->gb = cx->bb = 0;
    }
}


char *CInputScript::GetString(void)
{
    // Point to the string.
    char *str = (char *) &m_fileBuf[m_filePos];

	printf("string: %s\n",str);
    // Skip over the string.
    while (GetByte());

    return str;
}

U32 CInputScript::GetColor(void)
{
	U32 r = GetByte();
	U32 g = GetByte();
	U32 b = GetByte();
	return (r << 16) | (g << 8) | b;
}

void CInputScript::InitBits(void)
{
    // Reset the bit position and buffer.
    m_bitPos = 0;
    m_bitBuf = 0;
}


S32 CInputScript::GetSBits (S32 n)
// Get n bits from the string with sign extension.
{
    // Get the number as an unsigned value.
    S32 v = (S32) GetBits(n);

    // Is the number negative?
    if (v & (1L << (n - 1)))
    {
        // Yes. Extend the sign.
        v |= -1L << n;
    }

    return v;
}


U32 CInputScript::GetBits (S32 n)
// Get n bits from the stream.
{
    U32 v = 0;

	while (true)
    {
		//if (m_bitPos == 0)
		//	printf("bitPos is ZERO: m_bitBuf: %02x\n", m_bitBuf);

        S32 s = n - m_bitPos;
        if (s > 0)
        {
            // Consume the entire buffer
            v |= m_bitBuf << s;
            n -= m_bitPos;

            // Get the next buffer
            m_bitBuf = GetByte();
            m_bitPos = 8;
        }
        else
        {
         	// Consume a portion of the buffer
            v |= m_bitBuf >> -s;
            m_bitPos -= n;
            m_bitBuf &= 0xff >> (8 - m_bitPos);	// mask off the consumed bits
			//printf("GetBits: nBitsToRead:%d m_bitPos:%d m_bitBuf:%02x v:%d\n", nBitsToRead, m_bitPos, m_bitBuf, v);
            return v;
        }
    }
}


void CInputScript::ParseEnd(char *str)
{
    printf("%stagEnd\n", str);
}


void CInputScript::ParseShowFrame(char *str, U32 frame, U32 offset)
{
    printf("%stagShowFrame\n", str);
    printf("\n%s<----- dumping frame %d file offset 0x%04x ----->\n", str, frame + 1, offset);
}


void CInputScript::ParseFreeCharacter(char *str)
{
    U32 tagid = (U32) GetWord();
    printf("%stagFreeCharacter \ttagid %-5u\n", str, tagid);
}


void CInputScript::ParsePlaceObject(char *str)
{
    U32 tagid = (U32) GetWord();
    U32 depth = (U32) GetWord();
    
    printf("%stagPlaceObject \ttagid %-5u depth %-5u\n", str, tagid, depth);
    
    if (!m_dumpAll)
        return;
        
    MATRIX matrix;
	GetMatrix(&matrix);
    INDENT;
    printf("%spos matrix hex [ a_fixed   b_fixed] = [%5.3f   %5.3f]\n", str, (double)matrix.a/65536.0, (double)matrix.b/65536.0);
    INDENT;
    printf("%s               [ c_fixed   d_fixed]   [%5.3f   %5.3f]\n", str, (double)matrix.c/65536.0, (double)matrix.d/65536.0);
    INDENT;
    printf("%s               [tx_fixed  ty_fixed]   [%5.3f   %5.3f]\n", str, (double)matrix.tx/65536.0, (double)matrix.ty/65536.0);

    if ( m_filePos < m_tagEnd ) 
    {
        CXFORM cxform;
    	GetCxform(&cxform, false);
        INDENT;
        printf("%scolor transform hex [flags  ] = [%08x   ]\n", str, cxform.flags);
        INDENT;
        printf("%s                    [aa   ab]   [%04u   %04u]\n", str, cxform.aa, cxform.ab);
        INDENT;
        printf("%s                    [ra   rb]   [%04u   %04u]\n", str, cxform.ra, cxform.rb);
        INDENT;
        printf("%s                    [ga   gb]   [%04u   %04u]\n", str, cxform.ga, cxform.gb);
        INDENT;
        printf("%s                    [ba   bb]   [%04u   %04u]\n", str, cxform.ba, cxform.bb);
    }
}


void CInputScript::ParsePlaceObject2(char *str)
{
    U8 flags = GetByte();
    U32 depth = GetWord();

    printf("%stagPlaceObject2 \tflags %-5u depth %-5u ", str, (int) flags, (int) depth);

	if ( flags & splaceMove )
        printf("move ");

    // Get the tag if specified.
    if (flags & splaceCharacter)
    {
        U32 tag = GetWord();
        printf("tag %-5u\n", tag);
    }
	else
	{
        printf("\n");
	}
        
    if (!m_dumpAll)
        return;

    // Get the matrix if specified.
    if (flags & splaceMatrix)
    {
		// this one gets called

        MATRIX matrix;

    	GetMatrix(&matrix);
		INDENT;
		printf("%spos matrix hex [ a_fixed   b_fixed] = [%5.3f   %5.3f]\n", str, (double)matrix.a/65536.0, (double)matrix.b/65536.0);
		INDENT;
		printf("%s               [ c_fixed   d_fixed]   [%5.3f   %5.3f]\n", str, (double)matrix.c/65536.0, (double)matrix.d/65536.0);
		INDENT;
		printf("%s               [tx_fixed  ty_fixed]   [%5.3f   %5.3f]\n", str, (double)matrix.tx/20.0, (double)matrix.ty/20.0);
    }

    // Get the color transform if specified.
    if (flags & splaceColorTransform) 
    {
        CXFORM cxform;
    
    	GetCxform(&cxform, true);
        INDENT;
        printf("%scolor transform hex [flags  ] = [%08x   ]\n", str, cxform.flags);
        INDENT;
        printf("%s                    [aa   ab]   [%04u   %04u]\n", str, cxform.aa, cxform.ab);
        INDENT;
        printf("%s                    [ra   rb]   [%04u   %04u]\n", str, cxform.ra, cxform.rb);
        INDENT;
        printf("%s                    [ga   gb]   [%04u   %04u]\n", str, cxform.ga, cxform.gb);
        INDENT;
        printf("%s                    [ba   bb]   [%04u   %04u]\n", str, cxform.ba, cxform.bb);
    }        

    // Get the ratio if specified.
    if (flags & splaceRatio)
    {
        U32 ratio = GetWord();
        
        INDENT;
        printf("%ratio %u\n", ratio);
    }        

    // Get the clipdepth if specified.
    if (flags & splaceDefineClip) 
    {
	    U32 clipDepth = GetWord();
        INDENT;
        printf("clipDepth %i\n", clipDepth);
    }

    // Get the instance name
    if (flags & splaceName) 
    {
		char* pszName = GetString();
        INDENT;
        printf("instance name %s\n", pszName);
	}
}


void CInputScript::ParseRemoveObject(char *str)
{
    U32 tagid = (U32) GetWord();
    U32 depth = (U32) GetWord();
    
    printf("%stagRemoveObject \ttagid %-5u depth %-5u\n", str, tagid, depth);
}


void CInputScript::ParseRemoveObject2(char *str)
{
    U32 depth = (U32) GetWord();
    
    printf("%stagRemoveObject2 depth %-5u\n", str, depth);
}


void CInputScript::ParseSetBackgroundColor(char *str)
{
    U32 r = GetByte();
    U32 g = GetByte();
    U32 b = GetByte();
    U32 color = (r << 16) | (g << 8) | b;
    
    printf("%stagSetBackgroundColor \tRGB_HEX %06x\n", str, color);
}


void CInputScript::ParseDoAction(char *str, bool fPrintTag)
{
	if (fPrintTag)
	{
		printf("%stagDoAction\n",  str);
	}

    if (!m_dumpAll)
        return;

    for (;;) 
    {
    	// Handle the action
    	int actionCode = GetByte();
        INDENT;
        printf("%saction code 0x%02x ", str, actionCode);
    	if ( !actionCode )
    		break;

    	int len = 0;
    	if (actionCode & sactionHasLength) 
        {
    		len = GetWord();
            printf("has length %5u ", len);
        }        

    	S32 pos = m_filePos + len;

    	switch ( actionCode ) 
        {
    		// Handle timeline actions
    		case sactionGotoFrame:
                printf("gotoFrame %5u\n", GetWord());
    		    break;

    		case sactionGotoLabel: 
                printf("gotoLabel %s\n", &m_fileBuf[m_filePos]);
    		    break;
                
    		case sactionNextFrame:
                printf("gotoNextFrame\n");
    		    break;

    		case sactionPrevFrame:
                printf("gotoPrevFrame\n");
    		    break;

    		case sactionPlay:
                printf("play\n");
    		    break;

    		case sactionStop:
                printf("stop\n");
    		    break;

            case sactionWaitForFrame: {
    			int frame = GetWord();
    			int skipCount = GetByte();
                printf("waitForFrame %-5u skipCount %-5u\n", frame, skipCount);
                } break;

            case sactionGetURL: {
    			char *url = GetString();
    			char *target = GetString();
                printf("getUrl %s target %s\n", url, target);
                } break;

    		case sactionSetTarget:
                printf("setTarget %s\n", &m_fileBuf[m_filePos]);
    		    break;

    		case sactionStopSounds:
    			printf("stopSounds\n");
    		    break;

    		case sactionToggleQuality:
    			printf("toggleQuality\n");
    		    break;
    	}
    	m_filePos = pos;
    }
    printf("\n");
}


void CInputScript::ParseStartSound(char *str)
{
    U32 tagid = (U32) GetWord();

    printf("%stagStartSound \ttagid %-5u\n", str, tagid);
    
    if (!m_dumpAll)
        return;

    U32 code = GetByte();
    INDENT;   
	printf("%scode %-3u", str, code);

	if ( code & soundHasInPoint )
		printf(" inpoint %u ", GetDWord());
	if ( code & soundHasOutPoint )
		printf(" oupoint %u", GetDWord());
	if ( code & soundHasLoops )
		printf(" loops %u", GetDWord());

	printf("\n");
	if ( code & soundHasEnvelope ) 
    {
		int points = GetByte();

		for ( int i = 0; i < points; i++ ) 
        {
            printf("\n");
            INDENT;   
			printf("%smark44 %u", str, GetDWord());
			printf(" left chanel %u", GetWord());
			printf(" right chanel %u", GetWord());
            printf("\n");
		}
	}
}


void CInputScript::ParseStopSound(char *str)
{
    printf("%stagStopSound\n", str);
}


void CInputScript::ParseProtect(char *str)
{
    printf("%stagProtect\n", str);
}

BOOL CInputScript::ParseShapeRecord(char *str)
{
	// Determine if this is an edge.
	BOOL isEdge = (BOOL) GetBits(1);

	if (!isEdge)
	{
		// Handle a state change
		U16 flags = (U16) GetBits(5);
		//printf("not an edge: flags: %x\n", flags);

		// Are we at the end?
		if (flags == 0)
		{
			printf("\tEnd of shape.\n\n");
			return true;
		}

		// Process a move to.
		if (flags & eflagsMoveTo)
		{
			U16 nBits = (U16) GetBits(5);
			S32 x = GetSBits(nBits);
			S32 y = GetSBits(nBits);
			printf("\tmoveto: (%g,%g).\n", (double)x/20.0, (double)y/20.0);
		}
		// Get new fill info.
		if (flags & eflagsFill0)
		{
			int i = GetBits(m_nFillBits);
			printf("\tFillStyle0: %d\n", i);
		}
		if (flags & eflagsFill1)
		{
			int i = GetBits(m_nFillBits);
			printf("\tFillStyle1: %d\n", i);
		}
		// Get new line info
		if (flags & eflagsLine)
		{
			int i = GetBits(m_nLineBits);
			printf("\tLineStyle: %d\n", i);
		}
		// Check to get a new set of styles for a new shape layer.
		if (flags & eflagsNewStyles)
		{
			printf("\tFound more Style info\n");

			// Parse the style.
			ParseShapeStyle(str);

			// Reset.
			m_nFillBits = (U16) GetBits(4);
			m_nLineBits = (U16) GetBits(4);
		}
		if (flags & eflagsEnd)
			printf("\tEnd of shape.\n\n");
  
		return flags & eflagsEnd ? true : false;
	}
	else
	{
		if (GetBits(1))
		{
			// Handle a line
			U16 nBits = (U16) GetBits(4) + 2;	// nBits is biased by 2

			// Save the deltas
			if (GetBits(1))
			{
				// Handle a general line.
				S32 x = GetSBits(nBits);
				S32 y = GetSBits(nBits);
				printf("\tlineto: (%g,%g).\n", (double)x/20.0, (double)y/20.0);
			}
			else
			{
				// Handle a vert or horiz line.
				if (GetBits(1))
				{
					// Vertical line
					S32 x = GetSBits(nBits);
					printf("\tvertline: %g\n", (double)x/20.0);
				}
				else
				{
					// Horizontal line
					S32 y = GetSBits(nBits);
					printf("\thorzline: %g\n", (double)y/20.0);
				}
			}
		}
		else
		{
			//printf("\tFound a curve\n");

		 	// Handle a curve
			U16 nBits = (U16) GetBits(4) + 2;	// nBits is biased by 2

			// Get the control
			S32 cx = GetSBits(nBits);
			S32 cy = GetSBits(nBits);

			// Get the anchor
			S32 ax = GetSBits(nBits);
			S32 ay = GetSBits(nBits);

			printf("\tcurveto: (%g,%g)(%g,%g)\n", (double)cx/20.0, (double)cy/20.0, (double)ax/20.0, (double)ay/20.0);
		}

		return false;
	}
}

void CInputScript::ParseShapeStyle(char *str)
// 
{
	U16 i = 0;

	// Get the number of fills.
	U16 nFills = GetByte();

	//printf("*** nFills: %d\n", nFills);

	// Do we have a larger number?
	if (nFills == 255)
	{
		// Get the larger number.
		nFills = GetWord();
	}

	//printf("\tNumber of fill styles \t%u\n", nFills);

	// Get each of the fill style.
	for (i = 1; i <= nFills; i++)
	{
		U16 fillStyle = GetByte();

		printf("\tFill Style %u", i);

		if (fillStyle & fillGradient)
		{
			// Get the gradient matrix.
			MATRIX mat;
			GetMatrix(&mat);

			// Get the number of colors.
			U16 nColors = (U16) GetByte();

			// Get each of the colors.
			for (U16 j = 0; j < nColors; j++)
			{
				GetByte();
				GetColor();
			}

			printf("\tGradient Fill with %u colors\n", nColors);
		}
		else if (fillStyle & fillBits)
		{
			// Get the bitmap matrix.
			MATRIX mat;
			GetMatrix(&mat);

			printf("\tBitmap Fill\n");
		}
		else
		{
			// A solid color
			U32 color = GetColor();
    		printf("\tSolid Color Fill RGB_HEX %06x\n", color);
		}
	}

	// Get the number of lines.
	U16 nLines = GetByte();

//printf("*** nLines: %d\n", nLines);

	// Do we have a larger number?
	if (nLines == 255)
	{
		// Get the larger number.
		nLines = GetWord();
	}

	printf("\tNumber of line styles \t%u\n", nLines);

	// Get each of the line styles.
	for (i = 1; i <= nLines; i++)
	{
		U16 width = GetWord();
		U32 color = GetColor();
    
		printf("\tLine style %-5u width %g color RGB_HEX %06x\n", i, (double)width/20.0, color);
	}
}


void CInputScript::ParseDefineShape(char *str)
{
    U32 tagid = (U32) GetWord();
    printf("%stagDefineShape \ttagid %-5u\n", str, tagid);

    SRECT rect;
        
    // Get the frame information.
    GetRect(&rect);
	printf("\tShape bounding width \t%g\n", (double)(rect.xmax - rect.xmin) / 20.0);
	printf("\tShape bounding height \t%g\n", (double)(rect.ymax - rect.ymin) / 20.0);

	// ShapeWithStyle
	if (m_dumpAll)
	{
		BOOL atEnd = false;

		ParseShapeStyle(str);

		//printf("Getting FillBits and LineBits: m_filePos: %02x [%d]\n", m_filePos, m_bitPos);
		m_nFillBits = (U16) GetBits(4);
		m_nLineBits = (U16) GetBits(4);

		printf("m_nFillBits:%d  m_nLineBits:%d\n", m_nFillBits, m_nLineBits);

		while (!atEnd)
		{
			atEnd = ParseShapeRecord(str);
		}
	}
}


void CInputScript::ParseDefineShape2(char *str)
{
    U32 tagid = (U32) GetWord();

    printf("%stagDefineShape2 \ttagid %-5u\n", str, tagid);
}


void CInputScript::ParseDefineShape3(char *str)
{
    U32 tagid = (U32) GetWord();

    printf("%stagDefineShape3 \ttagid %-5u\n", str, tagid);
}


void CInputScript::S_DumpImageGuts(char *str)
{
    U32 lfCount = 0;                
    INDENT;        
	printf("%s----- dumping image details -----", str);
    while (m_filePos < m_tagEnd)
    {
        if ((lfCount % 16) == 0)
        {
            printf("\n");
            INDENT;        
        	printf("%s", str);
        }
        lfCount += 1;
        printf("%02x ", GetByte());
    }
    printf("\n");
}

void CInputScript::ParseDefineBits(char *str)
{
    U32 tagid = (U32) GetWord();

    printf("%stagDefineBits \ttagid %-5u\n", str, tagid);

    if (!m_dumpGuts)
        return;

    S_DumpImageGuts(str);
}


void CInputScript::ParseDefineBitsJPEG2(char *str)
{
    U32 tagid = (U32) GetWord();

    printf("%stagDefineBitsJPEG2 \ttagid %-5u\n", str, tagid);

    if (!m_dumpGuts)
        return;

    S_DumpImageGuts(str);
}


void CInputScript::ParseDefineBitsJPEG3(char *str)
{
    U32 tagid = (U32) GetWord();

    printf("%stagDefineBitsJPEG3 \ttagid %-5u\n", str, tagid);

    if (!m_dumpGuts)
        return;
        
    S_DumpImageGuts(str);
}


void CInputScript::ParseDefineBitsLossless(char *str)
{
    U32 tagid = (U32) GetWord();

    printf("%stagDefineBitsLossless tagid %-5u\n", str, tagid);
    
    if (!m_dumpAll)
        return;
     
    int format = GetByte();
	int width  =  GetWord();
	int height = GetWord();
    INDENT;        
	printf("%sformat %-3u width %-5u height %-5u\n", str, format, width, height);
        
    if (!m_dumpGuts)
        return;
        
    S_DumpImageGuts(str);
}


void CInputScript::ParseDefineBitsLossless2(char *str)
{
    U32 tagid = (U32) GetWord();

    printf("%stagDefineBitsLossLess2 \ttagid %-5u\n", str, tagid);

    if (!m_dumpAll)
        return;
     
    int format = GetByte();
	int width  =  GetWord();
	int height = GetWord();
    INDENT;        
	printf("%sformat %-3u width %-5u height %-5u\n", str, format, width, height);
        
    if (!m_dumpGuts)
        return;
        
    S_DumpImageGuts(str);
}


void CInputScript::ParseJPEGTables(char *str)
{
    printf("%stagJPEGTables\n", str);

    if (!m_dumpGuts)
        return;

    S_DumpImageGuts(str);
}


void CInputScript::ParseDefineButton(char *str)
{
    U32 tagid = (U32) GetWord();

    printf("%stagDefineButton \ttagid %-5u\n", str, tagid);
}

void CInputScript::where()
{
	printf("where: %04x [%02x]\n", m_filePos, m_fileBuf[m_filePos]);
}

void CInputScript::ParseButtonRecord(char *str, U32 iByte)
{
	U32 iPad = iByte >> 4;
	U32 iButtonStateHitTest = (iByte & 0x8);
	U32 iButtonStateDown = (iByte & 0x4);
	U32 iButtonStateOver = (iByte & 0x2);
	U32 iButtonStateUp = (iByte & 0x1);

	U32 iButtonCharacter = (U32)GetWord();
	U32 iButtonLayer = (U32)GetWord();

	printf("%s\tByte:%02x Pad:%02x HitTest:%02x Down:%02x Over:%02x Up:%02x\n",
			str, iByte, iPad, iButtonStateHitTest, iButtonStateDown, iButtonStateOver, iButtonStateUp);
	printf("%s\tiButtonCharacter:%d iButtonLayer:%d\n", str, iButtonCharacter, iButtonLayer);


    MATRIX matrix;
	GetMatrix(&matrix);

    printf("%s\tpos matrix hex [ a_fixed   b_fixed] = [%5.3f   %5.3f]\n", str, (double)matrix.a/65536.0, (double)matrix.b/65536.0);
    printf("%s\t               [ c_fixed   d_fixed]   [%5.3f   %5.3f]\n", str, (double)matrix.c/65536.0, (double)matrix.d/65536.0);
    printf("%s\t               [tx_fixed  ty_fixed]   [%5.3f   %5.3f]\n", str, (double)matrix.tx/65536.0, (double)matrix.ty/65536.0);

	int	nCharactersInButton = 1;

	for (int i=0; i<nCharactersInButton; i++)
	{
		CXFORM cxform;
		GetCxform(&cxform, true);	// ??could be false here??
		printf("%s\tcolor transform hex [flags  ] = [%08x   ]\n", str, cxform.flags);
		printf("%s\t                    [aa   ab]   [%04u   %04u]\n", str, cxform.aa, cxform.ab);
		printf("%s\t                    [ra   rb]   [%04u   %04u]\n", str, cxform.ra, cxform.rb);
		printf("%s\t                    [ga   gb]   [%04u   %04u]\n", str, cxform.ga, cxform.gb);
		printf("%s\t                    [ba   bb]   [%04u   %04u]\n", str, cxform.ba, cxform.bb);
	}
}

void CInputScript::ParseDefineButton2(char *str)
{
    U32 tagid = (U32) GetWord();

    printf("%stagDefineButton2 \ttagid %-5u\n", str, tagid);

	U32	iTrackAsMenu = (U32) GetByte();

	printf("%s\tTrackAsMenu: %d\n", str, iTrackAsMenu);


	// Get offset to first Action
	U32 iOffset = (U32) GetWord();
	U32	iActionStart = m_filePos + iOffset - 2;
	//printf("%s\tOffset: 0x%02x\n", str, iOffset);
	//printf("%s\t->>>ActionStart: 0x%02x\n", str, iActionStart);


	//
	// Parse Button Records
	//

	U32	iButtonEnd = (U32)GetByte();
	do
	{
		printf("%s\tButtonEnd: 0x%02x\n", str, iButtonEnd);
		ParseButtonRecord(str, iButtonEnd);
	}
	while ((iButtonEnd = (U32)GetByte()) != 0);

	printf("%s\t--end-of-buttons--\n", str);

	//
	// Parse Action Offset stuff
	//

	m_filePos = iActionStart;

	U32	iActionOffset = 0;
	while (true)
	{
		iActionOffset = (U32) GetWord();
		iActionStart = m_filePos + iActionOffset - 2;
		printf("%s\tActionOffset: %04x\n", str, iActionOffset);

		U32 iCondition = (U32) GetWord();
		printf("%s\tCondition: %04x\n", str, iCondition);

		ParseDoAction(str, false);

		if (iActionOffset == 0)
			break;

		m_filePos = iActionStart;
		//printf("%s\tShould be at: %04x\n", str, iActionStart);
	}

	printf("\n");
}


void CInputScript::ParseDefineFont(char *str)
{
    U32 tagid = (U32) GetWord();

    printf("%stagDefineFont \t\ttagid %-5u\n", str, tagid);
}


void CInputScript::ParseDefineMorphShape(char *str)
{
    U32 tagid = (U32) GetWord();

    printf("%stagDefineMorphShape \ttagid %-5u\n", str, tagid);
}

void CInputScript::ParseDefineFontInfo(char *str)
{
    U32 tagid = (U32) GetWord();

    printf("%stagDefineFontInfo \ttagid %-5u\n", str, tagid);
}


void CInputScript::ParseDefineFont2(char *str)
{
    U32 tagid = (U32) GetWord();

    printf("%stagDefineFont2 \ttagid %-5u\n", str, tagid);
}


void CInputScript::ParseDefineText(char *str)
{
    U32 tagid = (U32) GetWord();

    printf("%stagDefineText \t\ttagid %-5u\n", str, tagid);
}


void CInputScript::ParseDefineText2(char *str)
{
    U32 tagid = (U32) GetWord();

    printf("%stagDefineText2 \ttagid %-5u\n", str, tagid);
}


void CInputScript::ParseDefineSound(char *str)
{
    U32 tagid = (U32) GetWord();

    printf("%stagDefineSound \ttagid %-5u\n", str, tagid);
    
    if (!m_dumpAll)
        return;
        
    INDENT;        
    printf("%sSound format %u noSamples %u\n", str, GetByte(), GetWord());
}


void CInputScript::ParseDefineButtonSound(char *str)
{
    U32 tagid = (U32) GetWord();

    printf("%stagDefineButtonSound \ttagid %-5u\n", str, tagid);
    
    if (!m_dumpAll)
        return;
        
    // step through for button states
    for (int i = 0; i < 3; i++)
    {
        U32 soundTag = GetWord();
        switch (i)
        {
            case 0:         
                INDENT;   
                printf("%supState \ttagid %-5u\n", str, soundTag);
                break;
            case 1:            
                INDENT;   
                printf("%soverState \ttagid %-5u\n", str, soundTag);
                break;
            case 2:            
                INDENT;   
                printf("%sdownState \ttagid %-5u\n", str, soundTag);
                break;
        }
         
        if (soundTag)
        {
            U32 code = GetByte();
            INDENT;   
        	printf("%ssound code %u", str, code);

        	if ( code & soundHasInPoint )
        		printf(" inpoint %u", GetDWord());
        	if ( code & soundHasOutPoint )
        		printf(" outpoint %u", GetDWord());
        	if ( code & soundHasLoops )
        		printf(" loops %u", GetDWord());

			printf("\n");
			if ( code & soundHasEnvelope ) 
            {
        		int points = GetByte();

        		for ( int i = 0; i < points; i++ ) 
                {
                    printf("\n");
                    INDENT;   
        			printf("%smark44 %u", str, GetDWord());
        			printf(" left chanel %u", GetWord());
        			printf(" right chanel %u", GetWord());
					printf("\n");
        		}
        	}
        }
    }
        
}


void CInputScript::ParseSoundStreamHead(char *str)
{
	int mixFormat = GetByte();

	// The stream settings
	int format = GetByte();
	int nSamples = GetWord();

    printf("%stagSoundStreamHead \tmixFrmt %-3u frmt  %-3u nSamples %-5u\n", str, mixFormat, format, nSamples);
}


void CInputScript::ParseSoundStreamHead2(char *str)
{
	int mixFormat = GetByte();

	// The stream settings
	int format = GetByte();
	int nSamples = GetWord();

    printf("%stagSoundStreamHead \tmixFormat %-3u format %-3u nSamples %-5u\n", str, mixFormat, format, nSamples);
}

void CInputScript::ParseSoundStreamBlock(char *str)
{
    printf("%stagSoundStreamBlock\n", str);
}


void CInputScript::ParseDefineButtonCxform(char *str)
{
    U32 tagid = (U32) GetWord();

    printf("%stagDefineButtonCxform \ttagid %-5u\n", str, tagid);
    
    if (!m_dumpAll)
        return;
        
    while (m_filePos < m_tagEnd)
    {
        CXFORM cxform;
    	GetCxform(&cxform, false);
        INDENT;
        printf("%scolor transform hex [flags  ] = [%08x   ]\n", str, cxform.flags);
        INDENT;
        printf("%s                    [aa   ab]   [%04u   %04u]\n", str, cxform.aa, cxform.ab);
        INDENT;
        printf("%s                    [ra   rb]   [%04u   %04u]\n", str, cxform.ra, cxform.rb);
        INDENT;
        printf("%s                    [ga   gb]   [%04u   %04u]\n", str, cxform.ga, cxform.gb);
        INDENT;
        printf("%s                    [ba   bb]   [%04u   %04u]\n", str, cxform.ba, cxform.bb);
    }
}

void CInputScript::ParseNameCharacter(char *str)
{
    U32 tagid = (U32) GetWord();
    char *label = GetString();

    printf("%stagNameCharacter \ttagid %-5u label '%s'\n", str, tagid, label);
}


void CInputScript::ParseFrameLabel(char *str)
{
    char *label = GetString();

    printf("%stagFrameLabel lable \"%s\"\n", str, label);
}


void CInputScript::ParseDefineMouseTarget(char *str)
{
    printf("%stagDefineMouseTarget\n", str);
}


void CInputScript::ParseUnknown(char *str)
{
    DEBUG_ASSERT;
    printf("%sUnknown Tag\n", str);
}


void CInputScript::ParseTags(bool sprite, U32 tabs)
// Parses the tags within the file.
{

    char str[33];   // indent level
    
    {
        U32 i = 0;
    
        for (i = 0; i < tabs && i < 32; i++)
        {
            str[i] = '\t';
        }
        str[i] = 0;
    }        

    if (sprite)
    {
        U32 tagid = (U32) GetWord();
        U32 frameCount = (U32) GetWord();

        printf("%stagDefineSprite \ttagid %-5u \tframe count %-5u\n", str, tagid, frameCount);
    }
    else
    {        
		printf("\n%s<----- dumping frame %d file offset 0x%04x ----->\n", str, 0, m_filePos);
        
        // Set the position to the start position.
        m_filePos = m_fileStart;
    }        
    
    // Initialize the end of frame flag.
    BOOL atEnd = false;

    // Reset the frame position.
    U32 frame = 0;

    // Loop through each tag.
    while (!atEnd)
    {
        // Get the current tag.
        U16 code = GetTag();

		if (false)
		{
			printf("Tag dump: %04x: ", m_tagStart);
			for (U32 i=m_tagStart; i<m_tagEnd; i++)
				printf("%02x ", m_fileBuf[i]);
		}

        printf("%sTag: 0x%02x len: 0x%02x: ", str, code, m_tagLen);

        // Get the tag ending position.
        U32 tagEnd = m_tagEnd;

        switch (code)
        {
            case stagEnd:

                // Parse the end tag.
                ParseEnd(str);

                // We reached the end of the file.
                atEnd = true;

                break;
        
            case stagShowFrame:
                ParseShowFrame(str, frame, tagEnd);

                // Increment to the next frame.
                ++frame;
                break;

            case stagFreeCharacter:
                ParseFreeCharacter(str);
                break;

            case stagPlaceObject:
                ParsePlaceObject(str);
                break;

            case stagPlaceObject2:
                ParsePlaceObject2(str);
                break;

            case stagRemoveObject:
                ParseRemoveObject(str);
                break;

            case stagRemoveObject2:
                ParseRemoveObject2(str);
                break;

            case stagSetBackgroundColor:
                ParseSetBackgroundColor(str);
                break;

            case stagDoAction:
                ParseDoAction(str);
                break;

            case stagStartSound:
                ParseStartSound(str);
                break;

            case stagProtect:
                ParseProtect(str);
                break;

            case stagDefineShape: 
                ParseDefineShape(str);
                break;

            case stagDefineShape2:
                ParseDefineShape2(str);
                break;

            case stagDefineShape3:
                ParseDefineShape3(str);
                break;

            case stagDefineBits:
                ParseDefineBits(str);
                break;

            case stagDefineBitsJPEG2:
                ParseDefineBitsJPEG2(str);
                break;

            case stagDefineBitsJPEG3:
                ParseDefineBitsJPEG3(str);
                break;

            case stagDefineBitsLossless:
                ParseDefineBitsLossless(str);
                break;

            case stagDefineBitsLossless2:
                ParseDefineBitsLossless2(str);
                break;

            case stagJPEGTables:
                ParseJPEGTables(str);
                break;

            case stagDefineButton:
                ParseDefineButton(str);
                break;

            case stagDefineButton2:
                ParseDefineButton2(str);
                break;

            case stagDefineFont:
                ParseDefineFont(str);
                break;

            case stagDefineMorphShape:
                ParseDefineMorphShape(str);
                break;

            case stagDefineFontInfo:
                ParseDefineFontInfo(str);
                break;

            case stagDefineText:
                ParseDefineText(str);
                break;

            case stagDefineText2:
                ParseDefineText2(str);
                break;

            case stagDefineSound:
                ParseDefineSound(str);
                break;

            case stagDefineButtonSound:
                ParseDefineButtonSound(str);
                break;

            case stagSoundStreamHead:
                ParseSoundStreamHead(str);
                break;

            case stagDefineButtonCxform:
                ParseDefineButtonCxform(str);
                break;

            case stagDefineSprite:
                ParseTags(true, tabs + 1);
                break;

            case stagNameCharacter:
                ParseNameCharacter(str);
                break;

            case stagFrameLabel:
                ParseFrameLabel(str);
                break;

            case stagDefineFont2:
                ParseDefineFont2(str);
                break;

            default:
                ParseUnknown(str);
                break;
        }

        // Increment the past the tag.
        m_filePos = tagEnd;
    }
}


BOOL CInputScript::ParseFile(char * pInput)
{
    U8 fileHdr[8];
    BOOL sts = true;
    FILE *inputFile = NULL;

    // Free the buffer if it is there.
    if (m_fileBuf != NULL)
    {
        delete m_fileBuf;
        m_fileBuf = NULL;
        m_fileSize = 0;
    }

    //printf("***** Dumping SWF File Information *****\n");
    
	// Open the file for reading.
    inputFile = fopen(pInput, "rb");

    // Did we open the file?
    if (inputFile == NULL) 
    {
        sts = false;
		printf("ERROR: Can't open file %s\n", pInput);
    }

    // Are we OK?
    if (sts)
    {
        // Read the file header.
        if (fread(fileHdr, 1, 8, inputFile) != 8)
        {
            sts = false;
			printf("ERROR: Can't read the header of file %s\n", pInput);
        }
    }

    // Are we OK?
    if (sts)
    {
		printf("----- Reading the file header -----\n");

        // Verify the header and get the file size.
        if (fileHdr[0] != 'F' || fileHdr[1] != 'W' || fileHdr[2] != 'S' )
        {
			printf("ERROR: Illegal Header - not a Shockwave Flash File\n");

			// Bad header.
            sts = false;
        }
        else
        {
            // Get the file version.
            m_fileVersion = (U16) fileHdr[3];

			printf("FWS\n");
			printf("File version \t%u\n", m_fileVersion);
        }
    }
        
    // Are we OK?
    if (sts)
    {
        // Get the file size.
        m_fileSize = (U32) fileHdr[4] | ((U32) fileHdr[5] << 8) | ((U32) fileHdr[6] << 16) | ((U32) fileHdr[7] << 24);

		printf("File size \t%u\n", m_fileSize);

        // Verify the minimum length of a Flash file.
        if (m_fileSize < 21)
        {
			printf("ERROR: file size is too short\n");
            // The file is too short.
            sts = false;
        }
    }

    // Are we OK?
    if (sts)
    {
        // Allocate the buffer.
        m_fileBuf = new U8[m_fileSize];

        // Is there data in the file?
        if (m_fileBuf == NULL)
        {
            sts = false;
        }
    }
        
    // Are we OK?
    if (sts)
    {
        // Copy the data already read from the file.
        memcpy(m_fileBuf, fileHdr, 8);

        // Read the file into the buffer.
        if (fread(&m_fileBuf[8], 1, m_fileSize - 8, inputFile) != (m_fileSize - 8))
        {
            sts = false;
        }
    }

    // Do we have a file handle?
    if (inputFile != NULL)
    {
        // Close the file.
        fclose(inputFile);
        inputFile = NULL;
    }

    // Are we OK?
    if (sts)
    {
        SRECT rect;
        
        // Set the file position past the header and size information.
        m_filePos = 8;

        // Get the frame information.
        GetRect(&rect);
		printf("Movie width \t%u\n", (rect.xmax - rect.xmin) / 20);
		printf("Movie height \t%u\n", (rect.ymax - rect.ymin) / 20);

        U32 frameRate = GetWord() >> 8;
        printf("Frame rate \t%u\n", frameRate);

        U32 frameCount = GetWord();
        printf("Frame count \t%u\n", frameCount);

        // Set the start position.
        m_fileStart = m_filePos;	

		printf("\n----- Reading movie details -----\n");

        // Parse the tags within the file.
        ParseTags(false, 0);
        printf("\n***** Finished Dumping SWF File Information *****\n");
    }

    // Free the buffer if it is there.
    if (m_fileBuf != NULL)
    {
        delete m_fileBuf;
        m_fileBuf = NULL;
    }

    // Reset the file information.
    m_filePos = 0;
    m_fileSize = 0;
    m_fileStart = 0;
    m_fileVersion = 0;

    // Reset the bit position and buffer.
    m_bitPos = 0;
    m_bitBuf = 0;

    return sts;
}


int main (int argc, char *argv[])
// Main program.
{
    BOOL sts = true;
    CInputScript * pInputScript = NULL;
	char *fileName = 0;
	int i = 0;

    // Check the argument count.
    if (argc < 2)
    {
        fprintf(stderr, "usage: swfdump [options] inputFile\n");
        fprintf(stderr, "       -a dumps all the data associated with each tag\n");
        fprintf(stderr, "       -p dumpts partial data associated with each tag\n");

        // Bad arguments.
        sts = false;
		goto exit_gracefully;
    }

    // Create a flash script object.
    if ((pInputScript = new CInputScript()) == NULL)
    {
        // A memory error occured.
        sts = false;
		goto exit_gracefully;
    }


	for (i = 1; i < argc; i++)
	{
		char *str = argv[i];

		if (str[0] == '-')
		{
			switch (strlen(str))
			{
				case 0:
				case 1:
					break;

				default:
					if (str[1] == 'a')
						pInputScript->m_dumpAll = true;
					if (str[1] == 'p') 
					{
						pInputScript->m_dumpAll = true;
						pInputScript->m_dumpGuts = true;
					}
					break;
			}
		}
		else
		{
			fileName = argv[i];
		}
	}

			
    // Parse the file passed in.
	if (fileName)
		pInputScript->ParseFile(fileName);
	else
	{
        fprintf(stderr, "usage: swfdump [options] inputFile\n");
        fprintf(stderr, "       -a dumps all the data associated with each tag\n");
        fprintf(stderr, "       -p dumpts partial data associated with each tag\n");
	}

    delete pInputScript;
    pInputScript = NULL;

exit_gracefully:
    return sts ? 0 : 1;
}
