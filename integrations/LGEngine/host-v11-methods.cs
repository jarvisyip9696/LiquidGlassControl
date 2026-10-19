            // LiquidGlassControl protocol 1.1 adapter.
            void OnExtended(int type, uint seq, PayloadReader r)
            {
                uint id=r.U32(); HostSurface s=Find(id);
                if(s==null){Error(seq,WireError.NoSurface,"extended: no surface");return;}
                int[] v=new int[5];float push=0;Rectangle rect=Rectangle.Empty;
                GlassButton[] buttons=null;bool carry=false,valid=true;
                if(type==10){for(int i=0;i<5;i++)v[i]=r.I32();valid=v[0]>=0&&v[0]<=50&&v[1]>=0&&v[1]<=100&&v[2]>=0&&v[2]<=100&&v[3]>=-1&&v[3]<=160&&v[4]>=-1&&v[4]<=8;}
                else if(type==11){v[0]=r.I32();v[1]=r.I32();valid=v[0]>=0&&v[0]<=3&&v[1]>=0&&v[1]<=100;}
                else if(type==12){push=r.F32();valid=Finite(push)&&Math.Abs(push)<=100;}
                else if(type==13){
                    int flag=r.U8(),count=r.U16();carry=flag!=0;
                    if(flag>1||count>32){Error(seq,WireError.BadPayload,"buttons: invalid count");return;}
                    buttons=new GlassButton[count];
                    for(int i=0;i<count;i++){
                        GlassButton b=new GlassButton();b.Hit=ReadRect(r);b.Cx=r.F32();b.Cy=r.F32();b.Hx=r.F32();b.Hy=r.F32();b.Rad=r.F32();b.Style=r.I32();b.Mark=r.I32();b.MaskBox=ReadRect(r);uint size=r.U32();
                        valid=valid&&ValidRect(b.Hit)&&ValidRect(b.MaskBox)&&Finite(b.Cx)&&Finite(b.Cy)&&Finite(b.Hx)&&Finite(b.Hy)&&Finite(b.Rad)&&Math.Abs(b.Cx)<=16384&&Math.Abs(b.Cy)<=16384&&b.Hx>=0&&b.Hx<=16384&&b.Hy>=0&&b.Hy<=16384&&b.Rad>=0&&b.Rad<=16384;
                        if(size>65536||(size!=0&&(long)size!=(long)b.MaskBox.Width*b.MaskBox.Height)){valid=false;break;}
                        if(size>0){b.Mask=new byte[(int)size];for(int j=0;j<b.Mask.Length;j++)b.Mask[j]=(byte)r.U8();}buttons[i]=b;
                    }
                }
                else if(type==14){rect=ReadRect(r);valid=ValidRect(rect);}
                else if(type==15){v[0]=r.I32();v[1]=r.U8();v[2]=r.U8();valid=v[0]>=0&&v[0]<=60&&v[1]<=1&&v[2]<=1;}
                else if(type==16){v[0]=r.U8();valid=v[0]<=1;}
                if(r.Bad||!valid){Error(seq,WireError.BadPayload,"extended: invalid payload");return;}
                if(!PostHost(delegate{
                    if(s.Closing)return;SurfaceProps n=s.Cur.Copy();
                    if(type==10){n.MotionZoom=v[0];n.MotionLiquid=v[1];n.MotionBounce=v[2];n.MotionTravel=v[3];n.OsdStyle=v[4];}
                    else if(type==11){n.SloshStyle=v[0];n.SloshAmount=v[1];}
                    else if(type==12){s.State.AddKick(push);n.KickVer++;}
                    else if(type==13){n.Buttons=buttons;n.ButtonsCarry=carry;n.ButtonsVer++;}
                    else if(type==14){n.Pointer=rect;n.HasPointer=rect.Width>0&&rect.Height>0;}
                    else if(type==15){n.Fps=v[0];n.Adapt=v[1];n.SavePower=v[2];}
                    else if(type==16)n.Busy=v[0]!=0;
                    else if(type==17){n.WantShown=false;n.CutVer++;}
                    n.Version=++s.HostVer;s.Cur=n;s.State.Props=n;if(s.Slot!=null)s.Slot.Wake();
                }))Error(seq,WireError.Internal,"host stopping");
            }
            static Rectangle ReadRect(PayloadReader r){return new Rectangle(r.I32(),r.I32(),r.I32(),r.I32());}
            static bool ValidRect(Rectangle r){return r.X>=-4096&&r.X<=8192&&r.Y>=-4096&&r.Y<=8192&&r.Width>=0&&r.Width<=4096&&r.Height>=0&&r.Height<=4096;}
