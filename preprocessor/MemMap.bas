' **   RPN Scientific Calculator for MSP430
' *    Copyright (C) 2014 Joey Shepard
' *
' *    This program is free software: you can redistribute it and/or modify
' *    it under the terms of the GNU General Public License as published by
' *    the Free Software Foundation, either version 3 of the License, or
' *    (at your option) any later version.
' *
' *    This program is distributed in the hope that it will be useful,
' *    but WITHOUT ANY WARRANTY; without even the implied warranty of
' *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
' *    GNU General Public License for more details.
' *
' *    You should have received a copy of the GNU General Public License
' *    along with this program.  If not, see <http://www.gnu.org/licenses/>.

Const charset = "abcdefghijklmnopqrstuvwxyz_1234567890"

Type variable
   name As String
   level As Integer
End Type
'check is MM_x is valid

Sub Main()
   Dim MM_READ As String, MM_WRITE As String
   Dim MM_ON As Boolean, MM_LEVEL As Integer
   Dim MM_COUNT As Integer, MM_VARS() As variable
   Dim MM_DECLARE As Boolean, MM_COUNTER As Long
   Dim MM_GLOBALS As Boolean, MM_ASSIGN_LIST As String
   Dim CommentOn As Boolean, CommentBuff As String
   Dim Quote1On As Boolean, Quote2On As Boolean
   Dim InPath As String, OutPath As String
   
   MM_LEVEL = 1
   MM_ON = False
   MM_DECLARE = False
   MM_GLOBALS = False
   CommentOn = False
   Quote1On = False
   Quote2On = False
   
   ReDim MM_VARS(0)
   
   Dim inbuff As String, newbuff As String, finalbuff As String
   Dim linebuff As String, linebuff2 As String
   Dim buff1 As String, buff2 As String, buff3 As String
   Dim line As Integer
   
   'ChDir App.Path
   'commandbuff = "test.c test_out.c"
   commandbuff = Command
      
   If InStr(commandbuff, " ") = 0 Then
      MsgBox "Usage: MemMap IN_FILE OUT_FILE"
      End
   End If
   
   InPath = Left(commandbuff, InStr(commandbuff, " ") - 1)
   OutPath = Mid(commandbuff, Len(InPath) + 2)
   
   If InPath = "" Or OutPath = "" Then
      MsgBox "Usage: MemMap IN_FILE OUT_FILE"
      End
   End If
      
   If Dir(CurDir & "\" & InPath) = "" Then
      MsgBox """" & InPath & """ not found.", vbCritical
      End
   End If
   
   Open InPath For Input As #1
      Do Until EOF(1)
         DoEvents
      
         line = line + 1
         inbuff = ""
         Line Input #1, inbuff
         
         derp2 = derp1
         derp1 = inbuff
         

         newbuff = StripInput(inbuff, line)
         
         If Left(newbuff, 2) <> "//" Then
            CommentBuff = " "
            For i = 1 To Len(newbuff)
               CommentBuff = CommentBuff + Mid(newbuff, i, 1)
                  
               If Not CommentOn Then linebuff = linebuff + Mid(newbuff, i, 1)
               
               If Not Quote1On And Not Quote2On Then
                  If Not CommentOn Then
                     If CommentBuff = "/*" Or CommentBuff = "//" Then
                        linebuff = Left(linebuff, Len(linebuff) - 2)
                     End If
                     If CommentBuff = "//" Then Exit For
                     If Mid(newbuff, i, 1) = "{" Then
                        MM_LEVEL = MM_LEVEL + 1
                     ElseIf Mid(newbuff, i, 1) = "}" Then
                        For j = 0 To UBound(MM_VARS)
                           If MM_VARS(j).level = MM_LEVEL Then
                              MM_VARS(j).level = 0
                           End If
                        Next j
                        MM_LEVEL = MM_LEVEL - 1
                     ElseIf Mid(newbuff, i, 1) = "'" Then
                        Quote1On = True
                     ElseIf Mid(newbuff, i, 1) = """" Then
                        Quote2On = True
                     End If
                     
                     If CommentBuff = "/*" Then
                        CommentBuff = ""
                        CommentOn = True
                     End If
                  Else
                     If CommentBuff = "*/" Then
                        CommentOn = False
                     End If
                  End If
               ElseIf Quote1On Then
                  If Mid(newbuff, i, 1) = "'" Then
                     Quote1On = False
                  End If
               ElseIf Quote2On Then
                  If Mid(newbuff, i, 1) = """" Then
                     Quote2On = False
                  End If
               End If
               If CommentBuff <> "" Then CommentBuff = Mid(newbuff, i, 1)
            Next i
         ElseIf InStr(newbuff, "*/") Then
            linebuff = Mid(newbuff, InStr(newbuff, "*/") + 2)
            CommentOn = False
         End If
            
         If Not Ignore(linebuff) Then
            If InStr(linebuff, "#pragma MM_READ ") = 1 Then
               MM_READ = Mid(linebuff, 17)
            ElseIf InStr(linebuff, "#pragma MM_WRITE ") = 1 Then
               MM_WRITE = Mid(linebuff, 18)
            ElseIf linebuff = "#pragma MM_ON" Then
               MM_ON = True
            ElseIf linebuff = "#pragma MM_OFF" Then
               MM_ON = False
            ElseIf linebuff = "#pragma MM_DECLARE" Then
               MM_DECLARE = True
            ElseIf linebuff = "#pragma MM_GLOBALS" Then
               MM_GLOBALS = True
            ElseIf linebuff = "#pragma MM_END" Then
               MM_DECLARE = False
               MM_GLOBALS = False
            ElseIf linebuff = "#pragma MM_ASSIGN_GLOBALS" Then
               linebuff = linebuff & MM_ASSIGN_LIST
            ElseIf InStr(linebuff, "#pragma MM_VAR ") = 1 Then
               MM_VARS(UBound(MM_VARS)).level = MM_LEVEL
               MM_VARS(UBound(MM_VARS)).name = Mid(linebuff, 16)
               ReDim Preserve MM_VARS(UBound(MM_VARS) + 1)
            ElseIf InStr(linebuff, "#pragma MM_OFFSET ") = 1 Then
               MM_COUNTER = Val(Mid(linebuff, 19))
            ElseIf MM_ON = False Then
            ElseIf MM_DECLARE Or MM_GLOBALS And linebuff <> "" Then
               linebuff2 = linebuff
               k = InStrRev(linebuff, " ")
               buff1 = Left(linebuff, k)
               If InStr(linebuff, "[") Then
                  buff1 = buff1 + "*" + Mid(linebuff, k + 1, InStr(linebuff, "[") - k - 1)
                  buff1 = buff1 & "=(unsigned char*)" & MM_COUNTER & ";"
                  buff2 = Mid(linebuff, InStr(linebuff, "[") + 1)
                  MM_COUNTER = MM_COUNTER + Val(Left(buff2, Len(buff2) - 2))
               Else
                  buff1 = buff1 + "*" + Mid(linebuff, k + 1, Len(linebuff) - k - 1)
                  buff1 = buff1 & "=(unsigned char*)" & MM_COUNTER & ";"
                  MM_COUNTER = MM_COUNTER + 1
               End If
               
               MM_VARS(UBound(MM_VARS)).level = MM_LEVEL
               If InStr(linebuff, "[") Then
                  MM_VARS(UBound(MM_VARS)).name = Mid(linebuff, k + 1, InStr(linebuff, "[") - k - 1)
               Else
                  MM_VARS(UBound(MM_VARS)).name = Mid(linebuff, k + 1, Len(linebuff) - k - 1)
                  'MsgBox Mid(linebuff, k + 1, Len(linebuff) - k - 1)
               End If
               ReDim Preserve MM_VARS(UBound(MM_VARS) + 1)
               
               linebuff = buff1
               
               If MM_GLOBALS Then
                  linebuff = Mid(linebuff, InStr(linebuff, "*") + 1)
                  MM_ASSIGN_LIST = MM_ASSIGN_LIST & vbCrLf & linebuff
                  linebuff = Left(linebuff2, InStr(linebuff2, "[") - 1)
                  buff1 = Left(linebuff, InStrRev(linebuff, " "))
                  linebuff = buff1 & "*" & Mid(linebuff, InStrRev(linebuff, " ") + 1) & ";"
               End If
            Else
               Dim arraybuff As String, depth As Integer
               Dim foundend As Boolean
                   
               Do
                  foundend = True
                  For i = 1 To Len(linebuff)
                     If InStr(charset, LCase(Mid(linebuff, i, 1))) Then
                        arraybuff = arraybuff + Mid(linebuff, i, 1)
                     ElseIf Mid(linebuff, i, 1) = "[" Then
                        For j = 0 To UBound(MM_VARS)
                           If MM_VARS(j).level <> 0 And MM_VARS(j).name = arraybuff Then
                              depth = 0
                              For k = i + 1 To Len(linebuff)
                                 If Mid(linebuff, k, 1) = "[" Then
                                    depth = depth + 1
                                 ElseIf Mid(linebuff, k, 1) = "]" Then
                                    If depth = 0 Then
                                       If Mid(linebuff, k + 1, 1) = "=" And Mid(linebuff, k + 2, 1) <> "=" Then
                                          buff1 = Left(linebuff, i - Len(arraybuff) - 1) & MM_WRITE & "(" & arraybuff & "+"
                                          buff1 = buff1 + Mid(linebuff, i + 1, k - i - 1) & "," & Mid(linebuff, k + 2)
                                          buff1 = Left(buff1, Len(buff1) - 1) & ");"
                                       ElseIf InStr("+-*/%&|^", Mid(linebuff, k + 1, 1)) <> 0 And Mid(linebuff, k + 2, 1) = "=" Then
                                          buff1 = Left(linebuff, i - Len(arraybuff) - 1) & MM_WRITE & "(" & arraybuff & "+"
                                          buff1 = buff1 & Mid(linebuff, i + 1, k - i - 1) & "," & MM_READ & "(" & arraybuff & "+"
                                          buff1 = buff1 & Mid(linebuff, i + 1, k - i - 1) & ")" & Mid(linebuff, k + 1, 1) & "(" & Mid(linebuff, k + 3)
                                          buff1 = Left(buff1, Len(buff1) - 1) & "));"
                                       Else
                                          buff1 = Left(linebuff, i - Len(arraybuff) - 1) & MM_READ & "(" & arraybuff & "+"
                                          buff1 = buff1 + Mid(linebuff, i + 1, k - i - 1) + ")" + Mid(linebuff, k + 1)
                                       End If
                                       Exit For
                                    End If
                                    depth = depth - 1
                                 End If
                                 buff1 = buff1 + Mid(linebuff, k, 1)
                              Next k
                              'MsgBox linebuff & vbCrLf & buff1
                              linebuff = buff1
                              foundend = False
                              Exit For
                           End If
                        Next j
                        arraybuff = ""
                     Else
                        arraybuff = ""
                     End If
                  Next i 'loop through linebuffer
               Loop While foundend = False 'until all arrays are found
            End If 'if it's not any of the pragmas
            
         End If 'If its not ignore
         'If linebuff <> "" Then 'this excludes blank lines
            finalbuff = finalbuff + linebuff + vbCrLf
         'End If
         linebuff = ""
         
      Loop 'until eof(1)
   Close #1
   
   Open OutPath For Output As #1
      Print #1, finalbuff
   Close #1
   Exit Sub
ErrHandler:
   MsgBox "Error: " & Err, vbCritical
   End
End Sub

Function Ignore(buff As String)
Dim IgnoreList(2) As String
IgnoreList(0) = "//"
IgnoreList(1) = "#include "
IgnoreList(2) = "#define "
For i = 0 To 2
    If Left(buff, Len(IgnoreList(i))) = IgnoreList(i) Then
        Ignore = True
        Exit Function
    End If
Next i
Ignore = False
End Function
Function StripInput(buff As String, c As Integer)
   Dim rebuff As String, lastadded As String, space As Boolean
   Dim quote1 As Boolean, quote2 As Boolean
   quote1 = False
   quote2 = False
   space = False
   lastadded = " "
      
   If InStr(buff, "#") <> 0 Then
      If InStr(buff, "#define") = InStr(buff, "#") Then
         StripInput = buff
         Exit Function
      End If
   End If
   
   For i = 1 To Len(buff)
      If Not quote1 And Not quote2 Then
         If Mid(buff, i, 1) = """" Then
            quote1 = True
         ElseIf Mid(buff, i, 1) = "'" Then
            quote2 = True
         End If
      ElseIf quote1 And Mid(buff, i, 1) = """" Then
         quote1 = False
      ElseIf quote2 And Mid(buff, i, 1) = "'" Then
         quote2 = False
      End If
      
      If quote1 = True Or quote2 = True Then
         rebuff = rebuff + Mid(buff, i, 1)
         lastadded = Mid(buff, i, 1)
      Else
         If Mid(buff, i, 1) <> " " Then
            If InStr(charset, LCase(lastadded)) <> 0 And InStr(charset, LCase(Mid(buff, i, 1))) <> 0 And space Then
               rebuff = rebuff + " "
            End If
            rebuff = rebuff + Mid(buff, i, 1)
            lastadded = Mid(buff, i, 1)
            space = False
         Else
            space = True
         End If
      End If
   Next i
   If Right(rebuff, 1) = " " Then rebuff = Left(rebuff, Len(rebuff) - 1)

   StripInput = rebuff
End Function
