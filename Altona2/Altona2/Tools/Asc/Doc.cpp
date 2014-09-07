/****************************************************************************/
/***                                                                      ***/
/***   (C) 2012-2014 Dierk Ohlerich et al., all rights reserved.          ***/
/***                                                                      ***/
/***   Released under BSD 2 clause license, see LICENSE.TXT               ***/
/***                                                                      ***/
/****************************************************************************/

#include "Altona2/Libs/Base/Base.hpp"
#include "Altona2/Libs/Base/Graphics.hpp"
#include "Altona2/Libs/Base/DxShaderCompiler.hpp"
#include "Doc.hpp"

/****************************************************************************/

wDocument *Doc;

/****************************************************************************/
/***                                                                      ***/
/***                                                                      ***/
/***                                                                      ***/
/****************************************************************************/

wDocument::wDocument()
{
    sASSERT(Doc==0);
    Doc = this;

    Material = 0;
    Predicate = 0;
    Scan = new sScanner;
    Platform = 0;
    EnableDump = 0;
    ErrorFlag = 0;
}

wDocument::~wDocument()
{
    delete Scan;

    Permutes.DeleteAll();
    Predicates.DeleteAll();
    AscTypes.DeleteAll();
    AscMembers.DeleteAll();
    AscBuffers.DeleteAll();
    Materials.DeleteAll();
    Shaders.DeleteAll();

    sASSERT(Doc==this);
    Doc = 0;
}

/****************************************************************************/

sBool wDocument::Parse(const sChar *inputfilename,sInt platform,sInt dump)
{
    InputFilename = inputfilename;
    Platform = platform;
    EnableDump = dump?1:0;
    return AscParse(inputfilename);
}

void wDocument::PrintHeader(const sChar *prefix)
{
    Out.Clear();
    Out.PrintF("%s/****************************************************************************/\n",prefix);
    Out.PrintF("%s/***                                                                      ***/\n",prefix);
    Out.PrintF("%s/***   this file is automatically generated by asc.exe.                   ***/\n",prefix);
    Out.PrintF("%s/***   please do not edit.                                                ***/\n",prefix);
    Out.PrintF("%s/***                                                                      ***/\n",prefix);
    Out.PrintF("%s/****************************************************************************/\n",prefix);
}

const sChar *wDocument::GetAsm()
{
    GetCode(0);
    return Out.Get();
}

const sChar *wDocument::GetCpp()
{
    GetCode(1);
    return Out.Get();
}


wShader *wDocument::AddShader(wShader *sh)
{
    sh->Checksum.Calc(sh->Data,sh->Size);
    for(auto cmp : Shaders)
    {
        if(sh->Checksum==cmp->Checksum && sh->Size==cmp->Size)
        {
            if(sCmpMem(sh->Data,cmp->Data,sh->Size)==0)
            {
                delete sh;
                return cmp;
            }
        }
    }

    Shaders.Add(sh);
    return sh;
}

void wDocument::GetCode(sBool cpp)
{
    // compile all shaders

    for(auto mtrl : Materials)
    {
        Material = mtrl;
        sU32 maxperm = 1<<mtrl->PermuteShift;
        for(sInt stage=0;stage<sST_Max;stage++)
        {
            mtrl->Shaders[stage].SetSize((sInt)maxperm);
            for(sU32 j=0;j<maxperm;j++)
                mtrl->Shaders[stage][j] = 0;

            for(Permutation = 0;Permutation<maxperm;Permutation++)
            {
                if(mtrl->PermutationIsValid(Permutation))
                {
                    static const sChar *shadername[] = { "vs","hs","ds","gs","ps","cs" };

                    wShader *shader = 0;
                    if(Platform==sConfigRenderDX9)
                    {
                        if(!mtrl->Sources[wSL_Hlsl3][stage].Source.IsEmpty())
                        {
                            if(EnableDump)
                                Dump.PrintF("\n*** %s %d.%s ***\n\n",mtrl->Name,Permutation,shadername[stage]);
                            if(HlslParse(stage,mtrl->Sources[wSL_Hlsl3][stage].Source,mtrl->Sources[wSL_Hlsl3][stage].Line))
                            {
                                static const sChar *profiles[] = { "vs_3_0","??","??","??","ps_3_0","??" };
                                sTextBuffer errors;
                                sShaderBinary *bin = sCompileShaderDX((sShaderTypeEnum)stage,profiles[stage],Out.Get(),&errors);
                                if(bin)
                                    shader = new wShader(bin->Size,bin->Data);
                                else
                                    ErrorFlag = 1;
                                if(!bin)
                                {
                                    sPrintF("\n*** %s %d.%s ***\n\n",mtrl->Name,Permutation,shadername[stage]);
                                    sTextBuffer lines; lines.PrintWithLineNumbers(Out.Get()); sPrint(lines.Get());
                                }
                                sPrint(errors.Get());
                                sDPrint(errors.Get());
                                if(EnableDump)
                                    Dump.Print(errors.Get());

                                delete bin;
                            }
                        }
                    }
                    else if(Platform==sConfigRenderDX11)
                    {
                        if(!mtrl->Sources[wSL_Hlsl5][stage].Source.IsEmpty())
                        {
                            if(EnableDump)
                                Dump.PrintF("\n*** %s %d.%s ***\n\n",mtrl->Name,Permutation,shadername[stage]);
                            if(HlslParse(stage,mtrl->Sources[wSL_Hlsl5][stage].Source,mtrl->Sources[wSL_Hlsl5][stage].Line))
                            {
                                static const sChar *profiles[] = { "vs_5_0","hs_5_0","ds_5_0","gs_5_0","ps_5_0","cs_5_0" };
                                sTextBuffer errors;
                                sShaderBinary *bin = sCompileShaderDX((sShaderTypeEnum)stage,profiles[stage],Out.Get(),&errors);
                                if(bin)
                                    shader = new wShader(bin->Size,bin->Data);
                                else
                                    ErrorFlag = 1;
                                if(!bin)
                                {
                                    sPrintF("\n*** %s %d.%s ***\n\n",mtrl->Name,Permutation,shadername[stage]);
                                    sTextBuffer lines; lines.PrintWithLineNumbers(Out.Get()); sPrint(lines.Get());
                                }
                                sPrint(errors.Get());
                                sDPrint(errors.Get());
                                if(EnableDump)
                                    Dump.Print(errors.Get());

                                delete bin; 
                            }
                        }
                    }
                    else if(Platform==sConfigRenderGL2 || Platform==sConfigRenderGLES2 )
                    {
                        if(!mtrl->Sources[wSL_Glsl1][stage].Source.IsEmpty())
                        {
                            if(EnableDump)
                                Dump.PrintF("\n*** %s %d.%s ***\n\n",mtrl->Name,Permutation,shadername[stage]);
                            if(GlslParse(stage,mtrl->Sources[wSL_Glsl1][stage].Source,mtrl->Sources[wSL_Glsl1][stage].Line))
                            {
                                shader = new wShader(Out.GetCount()+1,(const sU8 *)Out.Get());
                            }
                        }
                    }
                    else
                    {
                        shader = new wShader(4,(sU8 *)"fake");
                        shader->Size = 4;
                    }
                    if(shader)
                        mtrl->Shaders[stage][Permutation] = AddShader(shader);
                }
            }
        }
    }

    // output header

    const sChar *prefix = Bits64 ? "" : "_";
    if(!cpp)
    {
        PrintHeader(";");
        Out.Print("\tsection\t.text\n");
        Out.PrintF("\tbits\t%d\n",Bits64 ? 64 : 32);
        Out.PrintF("\talign\t%d,db 0\n",Bits64 ? 8 : 4);
        Out.Print("\n");
        for(auto mtrl : Materials)
        {
            Out.PrintF("global %s%s_Array\n",prefix,mtrl->Name);
            Out.PrintF("global %s%s\n",prefix,mtrl->Name);
        }
        Out.Print("\n");
    }
    else
    {
        PrintHeader();
        Out.Print("#include \"Altona2/Libs/Base/Base.hpp\"\n");
        Out.Print("\n");
        Out.Print("using namespace Altona2;\n");
        Out.Print("\n");

        for(auto mtrl : Materials)
        {
            Out.PrintF("extern \"C\" Altona2::sAllShaderPara %s_Array[];\n",mtrl->Name);
            Out.PrintF("extern \"C\" Altona2::sAllShaderPermPara %s;\n",mtrl->Name);
        }
        Out.Print("\n");
    }

    // output all shaders

    for(auto sh : Shaders)
    {
        if(!cpp)
        {
            Out.PrintF("_Shader_%08X:",sh->Checksum);
            for(sU32 i=0;i<sh->Size;i++)
            {
                if((i&15)==0)
                    Out.Print("\n\tdb\t");
                Out.PrintF("0x%02x,",sh->Data[i]);
            }
            Out.Print("\n\n");
        }
        else
        {
            Out.PrintF("static unsigned char _Shader_%08X[] =\n",sh->Checksum);
            Out.Print("{");
            for(sU32 i=0;i<sh->Size;i++)
            {
                if((i&15)==0)
                    Out.Print("\n  ");
                Out.PrintF("%3d,",sh->Data[i]);
            }
            Out.Print("\n};\n\n");
        }
    }

    // output materials

    const sChar *type = Bits64 ? "dq" : "dd";
    for(auto mtrl : Materials)
    {
        sU32 maxperm = 1<<mtrl->PermuteShift;
        if(!cpp)
        {
            Out.PrintF("%s%s_Array:\n",prefix,mtrl->Name);
            for(sU32 i=0;i<maxperm;i++)
            {
                for(sInt stage=0;stage<sST_Max;stage++)
                {
                    if(mtrl->Shaders[stage][i])
                        Out.PrintF("\t%s\t_Shader_%08X,\n",type,mtrl->Shaders[stage][i]->Checksum);
                    else
                        Out.PrintF("\t%s\t0,\n",type);
                }
                Out.PrintF("\tdd\t");
                for(sInt stage=0;stage<sST_Max;stage++)
                    Out.PrintF("0x%08x,",mtrl->Shaders[stage][i] ? mtrl->Shaders[stage][i]->Size : 0);
                Out.Print("\n\n");
            }
            Out.Print("\n");
            Out.PrintF("%s%s:\n",prefix,mtrl->Name);
            Out.PrintF("\t%s\t%d\n",type,maxperm);
            Out.PrintF("\t%s\t%s%s_Array\n",type,prefix,mtrl->Name);
            Out.Print("\n");
        }
        else
        {
            Out.PrintF("Altona2::sAllShaderPara %s_Array[] =\n",mtrl->Name);
            Out.Print("{\n");
            for(sU32 i=0;i<maxperm;i++)
            {
                Out.Print("  {\n");
                Out.Print("    {\n");
                for(sInt stage=0;stage<sST_Max;stage++)
                {
                    if(mtrl->Shaders[stage][i])
                        Out.PrintF("      _Shader_%08X,\n",mtrl->Shaders[stage][i]->Checksum);
                    else
                        Out.PrintF("      0,\n");
                }
                Out.Print("    },\n");
                Out.Print("    {  ");
                for(sInt stage=0;stage<sST_Max;stage++)
                    Out.PrintF("0x%08x,",mtrl->Shaders[stage][i] ? mtrl->Shaders[stage][i]->Size : 0);
                Out.Print("    }\n");
                Out.Print("  },\n");
            }
            Out.Print("};\n");
            Out.Print("\n");
            Out.PrintF("Altona2::sAllShaderPermPara %s =\n",mtrl->Name);
            Out.Print("{\n");
            Out.PrintF("  %d,%s_Array\n",maxperm,mtrl->Name);
            Out.Print("};\n");
            Out.Print("\n");
        }
    }
}

const sChar *wDocument::GetHpp(const sChar *filename)
{
    sString<sMaxPath> HeaderProtectName;
    {
        HeaderProtectName = filename;
        sPtr size=0;
        sU8 *data = sLoadFile(filename,size);
        if(data)
        {
            sChecksumMD5 md;
            md.Calc(data,size);
            HeaderProtectName.PrintF("%08X",md);
        }
        delete[] data;
    }

    PrintHeader();
    Out.Print("\n");
    Out.PrintF("#ifndef FILE_%s_HPP\n",HeaderProtectName);
    Out.PrintF("#define FILE_%s_HPP\n",HeaderProtectName);
    Out.Print("\n");
    Out.Print("#include \"Altona2/Libs/Base/Base.hpp\"\n");
    Out.Print("\n");

    for(auto &i : NameSpace)
        Out.PrintF("namespace %s {\n",i);
    if(NameSpace.GetCount()>0)
        Out.PrintF("\n");

    Out.Print("/****************************************************************************/\n");
    Out.Print("\n");

    for(auto mtrl : Materials)
    {
        // buffers

        for(auto buf : mtrl->Buffers)
        {
            if(buf->Kind==wBK_Constants)
            {
                Out.PrintF("struct %s_%s\n",mtrl->Name,buf->Name);
                Out.Print("{\n");
                sInt temp = 0;
                sBool floatvector = 0;
                for(auto mem : buf->Members)
                {
                    const sChar *basename = "???";
                    switch(mem->Type->Base)
                    {
                    case wTB_Bool: 
                    case wTB_Int:
                    case wTB_Min16Int:
                    case wTB_Min12Int:
                        basename = "int"; 
                        break;

                    case wTB_UInt:
                    case wTB_Min16UInt:
                        basename = "Altona2::uint";
                        break;

                    case wTB_Float:
                    case wTB_Min16Float:
                    case wTB_Min10Float:
                        basename = "float";
                        floatvector = 1;
                        break;

                    case wTB_Double:
                        basename = "double";
                        break;
                    }

                    // special cases for matrices

                    if(floatvector && mem->Type->Rows==4 && mem->Type->Columns==4)
                    {
                        Out.PrintF("  Altona2::sMatrix44 %s",mem->Name);
                        if(mem->Type->Array)
                            Out.PrintF("[%d]",mem->Type->Array);
                    }
                    else if(floatvector && mem->Type->Rows==3 && mem->Type->Columns==4 && mem->Type->ColumnMajor==0)
                    {
                        Out.PrintF("  Altona2::sMatrix44A %s",mem->Name);
                        if(mem->Type->Array)
                            Out.PrintF("[%d]",mem->Type->Array);
                    }
                    else // normal cases
                    {
                        sInt rows = mem->Type->Rows;
                        sInt cols = mem->Type->Columns;
                        if(mem->Type->Rows>0 && mem->Type->ColumnMajor)
                            sSwap(rows,cols);
                        if(rows>0 || mem->Type->Array>0)
                            cols = 4;
                        if(floatvector && cols>=2)
                        {
                            Out.PrintF("  Altona2::sVector%d %s",cols,mem->Name);
                        }
                        else
                        {
                            Out.PrintF("  %s %s",basename,mem->Name);
                            if(cols>0)
                                Out.PrintF("[%d]",cols);
                        }
                        if(rows>0)
                            Out.PrintF("[%d]",rows);
                        if(mem->Type->Array>0)
                            Out.PrintF("[%d]",mem->Type->Array);
                        if(cols<4)
                        {
                            if(cols==3)
                                Out.PrintF(";\n  float _temp_%04d",temp++);
                            else
                                Out.PrintF(";\n  float _temp_%04d[%d]",temp++,4-sMax(1,cols));
                        }
                    }
                    Out.Print(";\n");
                }
                Out.Print("};\n");
            }
        }

        // permutes

        if(mtrl->Permutes.GetCount()>0)
        {
            Out.Print("\n");
            Out.PrintF("enum %s_PermuteEnum\n",mtrl->Name);
            Out.Print("{\n");
            for(auto perm : mtrl->Permutes)
            {
                Out.PrintF("  %s_%s = 0x%08x,\n",mtrl->Name,perm->Name,perm->Mask<<perm->Shift);
                for(auto &po : perm->Options)
                    Out.PrintF("  %s_%s_%s = 0x%08x,\n",mtrl->Name,perm->Name,po.Name,po.Value<<perm->Shift);
            }
            Out.Print("};\n");
        }

        // done

        Out.Print("\n");
        Out.PrintF("extern \"C\" Altona2::sAllShaderPermPara %s;\n",mtrl->Name);
        Out.Print("\n");
        Out.Print("/****************************************************************************/\n");
        Out.Print("\n");
        for(auto &i : NameSpace)
            Out.PrintF("};",i);
        if(NameSpace.GetCount()>0)
            Out.PrintF("\n");
        Out.Print("\n");
    }
    Out.Print("#endif\n");
    Out.Print("\n");

    return Out.Get();
}

const sChar *wDocument::GetDump()
{
    return Dump.Get();
}

/****************************************************************************/
