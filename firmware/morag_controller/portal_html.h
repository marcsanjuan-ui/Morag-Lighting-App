// MORAG Lighting - captive portal page served by morag_controller.ino
// Keep this file in the same folder as morag_controller.ino.
#pragma once
#include <pgmspace.h>

static const char PORTAL_HTML[] PROGMEM = R"MORAG_HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>MORAG Lighting</title>
<style>
:root{--p:#6C3BD1;--on-p:#fff;--pc:#EADDFF;--bg:#FEF7FF;--card:#fff;--tx:#1D1B20;--mu:#625B71;--ln:#CAC4D0;--err:#B3261E;--ok:#2E7D32}
@media (prefers-color-scheme:dark){:root{--p:#CFBCFF;--on-p:#381E72;--pc:#4F378B;--bg:#141218;--card:#211F26;--tx:#E6E0E9;--mu:#CAC4D0;--ln:#49454F;--err:#F2B8B5;--ok:#81C995}}
*{box-sizing:border-box}
html,body{margin:0;background:var(--bg);color:var(--tx);font:15px/1.4 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
.wrap{max-width:480px;margin:0 auto;padding:12px 16px 40px}
header{display:flex;align-items:center;gap:12px;padding:8px 0 12px}
header h1{flex:1;margin:0;font-size:20px;text-align:center}
.av{width:40px;height:40px;border-radius:50%;border:0;background:var(--pc);color:var(--tx);font-weight:700;font-size:16px;cursor:pointer}
.card{background:var(--card);border:1px solid var(--ln);border-radius:16px;padding:16px;margin:0 0 12px}
.step{display:flex;gap:12px;align-items:center;margin-bottom:12px}
.dot{width:28px;height:28px;border-radius:50%;display:grid;place-items:center;font-size:13px;background:var(--ln);color:var(--tx);flex:none}
.dot.ok{background:var(--p);color:var(--on-p)}
.t{font-weight:600}.s{font-size:13px;color:var(--mu)}.s.ok{color:var(--p)}
label{display:block;font-size:13px;color:var(--mu);margin:8px 0 4px}
input[type=text],input[type=password],select{width:100%;padding:11px 12px;border:1px solid var(--ln);border-radius:10px;background:transparent;color:var(--tx);font:inherit}
select option{color:#000}
.row{display:flex;gap:8px;align-items:center}
.row>*{flex:1}
button{font:inherit;cursor:pointer}
.btn{width:100%;padding:12px;border-radius:24px;border:0;background:var(--p);color:var(--on-p);font-weight:600;margin-top:12px}
.btn.tonal{background:var(--pc);color:var(--tx)}
.btn.out{background:transparent;border:1px solid var(--ln);color:var(--p)}
.btn:disabled{opacity:.5}
.mini{flex:none;width:auto;padding:10px 14px;margin:0}
.err{color:var(--err);font-size:13px;margin-top:8px;min-height:1em}
.ip{font-size:18px;font-weight:600;letter-spacing:.3px}
.center{text-align:center}
.wheel{position:relative;width:240px;height:240px;margin:8px auto;touch-action:none}
.wheel canvas{width:240px;height:240px;border-radius:50%;display:block}
.mk{position:absolute;width:22px;height:22px;margin:-11px 0 0 -11px;border-radius:50%;border:3px solid #fff;box-shadow:0 0 0 1px #0006;pointer-events:none}
.sl{margin:10px 4px}
.sl .row span{flex:none;color:var(--mu)}
.sl .row b{font-weight:500;text-align:center}
input[type=range]{width:100%;accent-color:var(--p)}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;margin-top:12px}
.tile{display:flex;flex-direction:column;gap:4px;cursor:pointer;user-select:none;-webkit-user-select:none;-webkit-touch-callout:none}
.sw{aspect-ratio:1/1;border-radius:16px;border:1px solid var(--ln)}
.tile.on .sw{outline:3px solid var(--p);outline-offset:1px}
.tile small{font-size:12px;text-align:center;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.add .sw{display:grid;place-items:center;border-style:dashed;color:var(--p);font-size:13px;text-align:center}
.dots{display:flex;gap:8px;justify-content:center;align-items:center;margin:8px 0}
.cdot{width:36px;height:36px;border-radius:50%;border:1px solid var(--ln);cursor:pointer;position:relative}
.cdot.sel{border:3px solid var(--p)}
.cdot i{position:absolute;top:-8px;right:-8px;width:18px;height:18px;border-radius:50%;background:var(--card);border:1px solid var(--ln);font:12px/16px sans-serif;text-align:center;font-style:normal}
.plus{width:36px;height:36px;border-radius:50%;border:1px dashed var(--ln);background:transparent;color:var(--p);font-size:20px}
.view{display:none}.view.on{display:block}
.sheet{position:fixed;inset:0;background:var(--bg);overflow:auto;display:none;z-index:5}
.sheet.on{display:block}
.top{display:flex;align-items:center;gap:8px;padding:8px 0 12px}
.top h2{flex:1;margin:0;font-size:18px}
.icon{border:0;background:transparent;color:var(--tx);font-size:22px;width:40px;height:40px;border-radius:50%}
.sw-toggle{position:relative;width:52px;height:32px;flex:none}
.sw-toggle input{opacity:0;width:0;height:0}
.sw-toggle span{position:absolute;inset:0;background:var(--ln);border-radius:16px;transition:.2s}
.sw-toggle span:before{content:"";position:absolute;width:24px;height:24px;left:4px;top:4px;border-radius:50%;background:#fff;transition:.2s}
.sw-toggle input:checked+span{background:var(--p)}
.sw-toggle input:checked+span:before{transform:translateX(20px)}
details{border-top:1px solid var(--ln);padding:10px 0}
summary{cursor:pointer;font-weight:600}
#toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%);background:var(--tx);color:var(--bg);padding:10px 16px;border-radius:10px;font-size:14px;opacity:0;transition:.25s;pointer-events:none;max-width:90%;z-index:9}
#toast.on{opacity:1}
.hint{font-size:12px;color:var(--mu);text-align:center}
</style>
</head>
<body>
<div class="wrap" id="main">

<!-- ============ SETUP ============ -->
<section id="v-setup" class="view on">
  <header><h1>MORAG Lighting</h1></header>

  <div class="card">
    <div class="step"><div class="dot" id="d1">1</div><div><div class="t">Connect to device AP</div><div class="s" id="s1">Checking...</div></div></div>
  </div>

  <div class="card">
    <div class="step"><div class="dot" id="d2">2</div><div><div class="t">Connect device to Wi-Fi</div><div class="s" id="s2">-</div></div></div>
    <label for="ssidSel">SSID</label>
    <div class="row"><select id="ssidSel"><option value="">Tap Scan to find networks</option></select><button class="btn tonal mini" id="scanBtn">Scan</button></div>
    <input type="text" id="ssid" placeholder="or type the Wi-Fi name" style="margin-top:8px" autocapitalize="off" autocomplete="off">
    <label for="wpass">Password</label>
    <input type="password" id="wpass" autocomplete="off">
    <button class="btn tonal" id="provBtn">Connect to Wi-Fi</button>
    <div class="err" id="e2"></div>
  </div>

  <div class="card center">
    <div class="s">IP address</div>
    <div class="ip" id="ip">192.168.4.1</div>
    <div class="s" id="ipNote">On the device AP</div>
  </div>

  <div class="card">
    <div class="t center">User login</div>
    <label for="user">Username</label>
    <input type="text" id="user" autocapitalize="off" autocomplete="username">
    <label for="pass">Password</label>
    <input type="password" id="pass" autocomplete="current-password">
    <button class="btn" id="loginBtn">Sign in</button>
    <div class="err" id="e4"></div>
  </div>
</section>

<!-- ============ HOME ============ -->
<section id="v-home" class="view">
  <header>
    <button class="av" id="avBtn" title="Device profile">?</button>
    <h1>Home</h1>
    <label class="sw-toggle" title="Power"><input type="checkbox" id="power" checked><span></span></label>
  </header>
  <div class="hint" id="now">Now showing: -</div>

  <div class="wheel" id="wheel"><canvas width="240" height="240"></canvas><div class="mk"></div></div>
  <div class="row" style="max-width:240px;margin:0 auto"><input type="text" id="hex" maxlength="7" value="#FFFFFF" style="text-align:center"></div>

  <div class="sl"><div class="row"><b>Speed</b><span id="spV">50</span></div><input type="range" id="speed" min="1" max="100" value="50"></div>
  <div class="sl"><div class="row"><b>Brightness</b><span id="brV">60%</span></div><input type="range" id="bright" min="0" max="100" value="60"></div>
  <div class="row" style="margin:8px 4px"><span style="flex:none">Effects:</span><select id="effect"></select></div>

  <h3 class="center" style="margin:24px 0 0">Brands</h3>
  <div class="hint">Tap to apply &middot; long-press to edit &middot; saved on this controller</div>
  <div class="grid" id="grid"></div>
</section>

</div>

<!-- ============ BRAND EDITOR ============ -->
<div class="sheet" id="v-brand"><div class="wrap">
  <div class="top"><button class="icon" id="bBack">&larr;</button><h2 id="bTitle">Add brand</h2><button class="icon" id="bDel" title="Delete">&#128465;</button></div>
  <label for="bName">Name</label>
  <input type="text" id="bName" maxlength="24" style="text-align:center">
  <div class="sl"><div class="row"><b>Speed</b><span id="bSpV">50</span></div><input type="range" id="bSpeed" min="1" max="100" value="50"></div>
  <div class="sl"><div class="row"><b>Brightness</b><span id="bBrV">70%</span></div><input type="range" id="bBright" min="0" max="100" value="70"></div>
  <div class="dots" id="bDots"></div>
  <div class="wheel" id="bWheel"><canvas width="240" height="240"></canvas><div class="mk"></div></div>
  <div class="row" style="max-width:240px;margin:0 auto"><input type="text" id="bHex" maxlength="7" style="text-align:center"></div>
  <div class="row" style="margin:12px 4px"><span style="flex:none">Effects:</span><select id="bEffect"></select></div>
  <div class="hint">LEDs show color differently from screens. Preview on the strip and adjust by eye.</div>
  <div class="row"><button class="btn out" id="bPrev">Preview</button><button class="btn" id="bSave">Save</button></div>
</div></div>

<!-- ============ PROFILE ============ -->
<div class="sheet" id="v-prof"><div class="wrap">
  <div class="top"><button class="icon" id="pBack">&larr;</button><h2>Device profile</h2></div>
  <div class="card">
    <div class="t" id="pUser">Signed in</div>
    <div class="s" id="pAp"></div>
    <div class="s" id="pSite"></div>
    <div class="s" id="pIp"></div>
  </div>
  <div class="card">
    <details><summary>Device Wi-Fi</summary>
      <label for="pSsid">SSID</label><input type="text" id="pSsid" autocapitalize="off">
      <label for="pWpass">Password</label><input type="password" id="pWpass">
      <button class="btn tonal" id="pProv">Connect device to this Wi-Fi</button>
    </details>
    <details><summary>Change login</summary>
      <label for="pOld">Current password</label><input type="password" id="pOld">
      <label for="pNewU">New username</label><input type="text" id="pNewU" autocapitalize="off">
      <label for="pNewP">New password (6+)</label><input type="password" id="pNewP">
      <button class="btn" id="pAcc">Save login</button>
    </details>
    <details><summary>Device AP name and password</summary>
      <label for="pApS">AP name</label><input type="text" id="pApS" maxlength="32">
      <label for="pApP">AP password (8+)</label><input type="password" id="pApP">
      <button class="btn tonal" id="pApBtn">Save and restart device</button>
    </details>
    <button class="btn out" id="pOut">Sign out</button>
    <div class="hint" style="margin-top:12px">Forgot the login? While the device is on, hold BOOT for 5 seconds. It flashes white and restarts with the defaults.</div>
  </div>
</div></div>

<div id="toast"></div>

<script>
(function(){
"use strict";
var $=function(id){return document.getElementById(id)};
var EFFECTS=[["solid","Solid"],["breathe","Breathing"],["wipe","Color wipe"],["chase","Chase"],["gradient","Gradient"],["rainbow","Rainbow"]];
var T=null, USER="", info=null, brands=[], st={power:true,bright:60,effect:"solid",speed:50,c:["#FFFFFF","#000000","#000000"],name:"Custom"};
var activeBrand=-1, editIdx=-1, ed=null, slot=0;

function toast(m){var t=$("toast");t.textContent=m;t.className="on";clearTimeout(toast.h);toast.h=setTimeout(function(){t.className=""},2600)}
function show(v){$("v-setup").className="view"+(v==="setup"?" on":"");$("v-home").className="view"+(v==="home"?" on":"")}
function sheet(id,on){$(id).className="sheet"+(on?" on":"");var any=$("v-brand").className.indexOf(" on")>0||$("v-prof").className.indexOf(" on")>0;$("main").style.display=any?"none":""}

function post(path,data){
  var b=new URLSearchParams(data||{}); if(T) b.set("t",T);
  return fetch(path,{method:"POST",body:b,cache:"no-store"}).then(function(r){
    return r.text().then(function(x){
      if(r.status===401&&T&&path!=="/api/login"&&path!=="/api/provision"){T=null;sheet("v-brand",0);sheet("v-prof",0);show("setup");toast("Session ended. Sign in again.")}
      if(!r.ok) throw new Error(x.replace(/^ERR,/,"")||("Error "+r.status));
      return x;
    });
  },function(){throw new Error("Cannot reach the device.")});
}

// ---------- command queue (latest wins per kind) ----------
var pending={}, order=[], busy=false;
function send(k,c){if(!(k in pending))order.push(k);pending[k]=c;pump()}
function pump(){
  if(busy||!order.length)return; busy=true;
  var k=order.shift(), c=pending[k]; delete pending[k];
  post("/api/cmd",{c:c}).catch(function(e){toast(e.message)}).then(function(){busy=false;setTimeout(pump,30)});
}

// ---------- colors ----------
function clamp(v,a,b){return v<a?a:v>b?b:v}
function hex2rgb(h){h=h.replace("#","");var v=parseInt(h,16);return [v>>16&255,v>>8&255,v&255]}
function rgb2hex(r,g,b){return "#"+((1<<24)|(r<<16)|(g<<8)|b).toString(16).slice(1).toUpperCase()}
function validHex(h){return /^#?[0-9a-fA-F]{6}$/.test(h)}
function normHex(h){h=h.trim();if(h[0]!=="#")h="#"+h;return h.toUpperCase()}
function hsv2rgb(h,s,v){var f=function(n){var k=(n+h/60)%6;return Math.round(255*(v-v*s*Math.max(0,Math.min(k,4-k,1))))};return [f(5),f(3),f(1)]}
function rgb2hs(r,g,b){r/=255;g/=255;b/=255;var mx=Math.max(r,g,b),mn=Math.min(r,g,b),d=mx-mn,h=0;
  if(d){if(mx===r)h=((g-b)/d)%6;else if(mx===g)h=(b-r)/d+2;else h=(r-g)/d+4;h*=60;if(h<0)h+=360}
  return [h,mx?d/mx:0]}

// ---------- color wheel ----------
function Wheel(el,onPick){
  var cv=el.querySelector("canvas"),mk=el.querySelector(".mk"),S=cv.width,R=S/2,ctx=cv.getContext("2d");
  var img=ctx.createImageData(S,S),d=img.data;
  for(var y=0;y<S;y++)for(var x=0;x<S;x++){
    var dx=x-R+.5,dy=y-R+.5,r=Math.sqrt(dx*dx+dy*dy),i=(y*S+x)*4;
    if(r>R)continue;
    var h=(Math.atan2(dy,dx)*180/Math.PI+360)%360,c=hsv2rgb(h,r/R,1);
    d[i]=c[0];d[i+1]=c[1];d[i+2]=c[2];d[i+3]=r>R-1?Math.round(255*(R-r)):255;
  }
  ctx.putImageData(img,0,0);
  function place(hex){var c=hex2rgb(hex),hs=rgb2hs(c[0],c[1],c[2]),a=hs[0]*Math.PI/180,rr=hs[1]*(R-2);
    mk.style.left=(R+Math.cos(a)*rr)+"px";mk.style.top=(R+Math.sin(a)*rr)+"px";mk.style.background=hex}
  function pick(e){var b=cv.getBoundingClientRect(),x=(e.clientX-b.left)*S/b.width-R,y=(e.clientY-b.top)*S/b.height-R;
    var r=Math.min(Math.sqrt(x*x+y*y),R),h=(Math.atan2(y,x)*180/Math.PI+360)%360,c=hsv2rgb(h,r/R,1),hex=rgb2hex(c[0],c[1],c[2]);
    place(hex);onPick(hex)}
  var down=false;
  el.addEventListener("pointerdown",function(e){down=true;try{el.setPointerCapture(e.pointerId)}catch(_){ }pick(e);e.preventDefault()});
  el.addEventListener("pointermove",function(e){if(down)pick(e)});
  el.addEventListener("pointerup",function(){down=false});
  el.addEventListener("pointercancel",function(){down=false});
  return {set:place};
}

// ---------- setup screen ----------
function stepUi(n,ok,txt){$("d"+n).className="dot"+(ok?" ok":"");$("d"+n).textContent=ok?"✓":n;$("s"+n).textContent=txt;$("s"+n).className="s"+(ok?" ok":"")}
function siteText(i){
  return {connected:"Connected to "+i.sta_ssid,connecting:"Connecting to "+i.sta_ssid+"...",failed:"Could not join "+i.sta_ssid+". Check the password.",
          not_found:i.sta_ssid+" not found. 2.4 GHz only.",none:"Not set up"}[i.sta_status]||i.sta_status}
function loadInfo(){
  return fetch("/api/info",{method:"POST",cache:"no-store"}).then(function(r){return r.json()}).then(function(i){
    info=i;
    stepUi(1,true,i.via_ap?"Connected to "+i.ap_ssid:"Not needed: reached over "+(i.sta_ssid||"the network"));
    var up=i.sta_status==="connected"&&i.sta_ip;
    stepUi(2,up,siteText(i));
    $("ip").textContent=up?i.sta_ip:i.ap_ip;
    $("ipNote").textContent=up?("On "+i.sta_ssid+" (on the device AP: "+i.ap_ip+")"):"On the device AP";
    if(!$("ssid").value&&i.sta_ssid)$("ssid").value=i.sta_ssid;
    return i;
  }).catch(function(){stepUi(1,false,"Not connected");stepUi(2,false,"Connect to the device first")});
}

$("scanBtn").onclick=function(){
  var b=this,tries=0;b.disabled=true;b.textContent="...";
  (function poll(){
    post("/api/scan").then(function(x){
      var j=JSON.parse(x);
      if(j.status==="scanning"&&tries++<12){setTimeout(poll,1200);return}
      var sel=$("ssidSel"),seen={};sel.innerHTML="";
      var o=document.createElement("option");o.value="";o.textContent=(j.networks&&j.networks.length)?"Choose a network":"No networks found";sel.appendChild(o);
      (j.networks||[]).forEach(function(n){if(!n.ssid||seen[n.ssid])return;seen[n.ssid]=1;
        var o=document.createElement("option");o.value=n.ssid;o.textContent=n.ssid+"  ("+n.rssi+" dBm"+(n.open?", open":"")+")";sel.appendChild(o)});
      b.disabled=false;b.textContent="Scan";
    }).catch(function(e){$("e2").textContent=e.message;b.disabled=false;b.textContent="Scan"});
  })();
};
$("ssidSel").onchange=function(){if(this.value)$("ssid").value=this.value};

function provision(ssid,pass,errEl,btn){
  if(!ssid){errEl.textContent="Enter the Wi-Fi name.";return}
  errEl.textContent="";btn.disabled=true;var old=btn.textContent;btn.textContent="Connecting...";
  post("/api/provision",{ssid:ssid,pass:pass}).then(function(){
    var n=0;(function poll(){setTimeout(function(){
      loadInfo().then(function(i){
        if(i&&(i.sta_status==="connected"||i.sta_status==="failed"||i.sta_status==="not_found")||n++>16){btn.disabled=false;btn.textContent=old;if(i)toast(siteText(i));fillProfile();return}
        poll();
      });
    },1500)})();
  }).catch(function(e){errEl.textContent=e.message;btn.disabled=false;btn.textContent=old});
}
$("provBtn").onclick=function(){provision($("ssid").value.trim(),$("wpass").value,$("e2"),this)};

$("loginBtn").onclick=function(){
  var u=$("user").value.trim(),p=$("pass").value,b=this;
  if(!u||!p){$("e4").textContent="Enter your username and password.";return}
  $("e4").textContent="";b.disabled=true;
  post("/api/login",{user:u,pass:p}).then(function(x){
    T=JSON.parse(x).token;USER=u;$("pass").value="";
    $("avBtn").textContent=u[0].toUpperCase();
    return Promise.all([refreshState(),loadBrands()]);
  }).then(function(){show("home")}).catch(function(e){$("e4").textContent=e.message}).then(function(){b.disabled=false});
};
$("pass").onkeydown=function(e){if(e.key==="Enter")$("loginBtn").click()};

// ---------- home ----------
var wheel=Wheel($("wheel"),function(hex){setColor(hex)});
function fillEffects(sel){EFFECTS.forEach(function(e){var o=document.createElement("option");o.value=e[0];o.textContent=e[1];sel.appendChild(o)})}
fillEffects($("effect"));fillEffects($("bEffect"));

function renderHome(){
  $("power").checked=st.power;$("now").textContent="Now showing: "+st.name;
  $("speed").value=st.speed;$("spV").textContent=st.speed;
  $("bright").value=st.bright;$("brV").textContent=st.bright+"%";
  $("effect").value=st.effect;$("hex").value=st.c[0];wheel.set(st.c[0]);
  renderGrid();
}
function refreshState(){
  return post("/api/status").then(function(x){var p=x.trim().split(",");if(p[0]!=="STATE")return;
    st={power:p[1]==="1",bright:+p[2],effect:p[3],speed:+p[4],c:[p[5],p[6],p[7]],name:p.slice(8).join(",")};renderHome()});
}
function setColor(hex){st.c[0]=hex;st.effect="solid";st.name="Custom";activeBrand=-1;$("hex").value=hex;$("effect").value="solid";
  $("now").textContent="Now showing: Custom";$("power").checked=st.power=true;renderGrid();send("color","COLOR,"+hex)}
$("hex").onchange=function(){if(validHex(this.value)){var h=normHex(this.value);wheel.set(h);setColor(h)}else this.value=st.c[0]};
$("power").onchange=function(){st.power=this.checked;send("power","POWER,"+(st.power?1:0))};
$("speed").oninput=function(){st.speed=+this.value;$("spV").textContent=st.speed;send("effect","EFFECT,"+st.effect+","+st.speed)};
$("bright").oninput=function(){st.bright=+this.value;$("brV").textContent=st.bright+"%";send("bright","BRIGHT,"+st.bright)};
$("effect").onchange=function(){st.effect=this.value;activeBrand=-1;renderGrid();send("effect","EFFECT,"+st.effect+","+st.speed)};

// ---------- brands ----------
function swatchBg(cols){var c=cols.filter(Boolean);if(c.length<2)c=[c[0]||"#888888",c[0]||"#888888"];return "linear-gradient(135deg,"+c.join(",")+")"}
function loadBrands(){return post("/api/brands").then(function(x){try{brands=JSON.parse(x)||[]}catch(_){brands=[]}renderGrid()})}
function saveBrands(){return post("/api/brands/save",{data:JSON.stringify(brands)})}
function presetCmd(b){return "PRESET,"+b.name.replace(/,/g," ").trim()+","+b.effect+","+b.speed+","+b.brightness+","+b.colors.slice(0,3).join(",")}
function longPress(el,onTap,onLong){
  var t=null,long=false;
  el.addEventListener("pointerdown",function(){long=false;t=setTimeout(function(){long=true;onLong()},550)});
  ["pointerup","pointerleave","pointercancel"].forEach(function(ev){el.addEventListener(ev,function(){clearTimeout(t)})});
  el.addEventListener("click",function(){if(!long)onTap()});
  el.addEventListener("contextmenu",function(e){e.preventDefault()});
}
function renderGrid(){
  var g=$("grid");g.innerHTML="";
  brands.forEach(function(b,i){
    var t=document.createElement("div");t.className="tile"+(i===activeBrand?" on":"");
    var sw=document.createElement("div");sw.className="sw";sw.style.background=swatchBg(b.colors);
    var n=document.createElement("small");n.textContent=b.name;
    t.appendChild(sw);t.appendChild(n);
    longPress(t,function(){applyBrand(i)},function(){openEditor(i)});
    g.appendChild(t);
  });
  var a=document.createElement("div");a.className="tile add";
  a.innerHTML='<div class="sw">+<br>add brand</div><small>&nbsp;</small>';
  a.onclick=function(){openEditor(-1)};g.appendChild(a);
}
function applyBrand(i){var b=brands[i];activeBrand=i;
  st={power:true,bright:b.brightness,effect:b.effect,speed:b.speed,c:[b.colors[0],b.colors[1]||"#000000",b.colors[2]||"#000000"],name:b.name};
  renderHome();send("preset",presetCmd(b))}

// ---------- brand editor ----------
var bWheel=Wheel($("bWheel"),function(hex){ed.colors[slot]=hex;$("bHex").value=hex;renderDots()});
function openEditor(i){
  editIdx=i;var src=i>=0?brands[i]:{name:"",colors:["#6C3BD1"],effect:"solid",speed:50,brightness:70};
  ed=JSON.parse(JSON.stringify(src));slot=0;
  $("bTitle").textContent=i>=0?"Edit brand":"Add brand";$("bDel").style.visibility=i>=0?"visible":"hidden";
  $("bName").value=ed.name;$("bSpeed").value=ed.speed;$("bSpV").textContent=ed.speed;
  $("bBright").value=ed.brightness;$("bBrV").textContent=ed.brightness+"%";$("bEffect").value=ed.effect;
  renderDots();sheet("v-brand",1);window.scrollTo(0,0);
}
function renderDots(){
  var d=$("bDots");d.innerHTML="";var l=document.createElement("span");l.textContent="Color";d.appendChild(l);
  ed.colors.forEach(function(c,i){var e=document.createElement("div");e.className="cdot"+(i===slot?" sel":"");e.style.background=c;
    e.onclick=function(){slot=i;renderDots()};
    if(i>0){var x=document.createElement("i");x.textContent="×";x.onclick=function(ev){ev.stopPropagation();ed.colors.splice(i,1);slot=0;renderDots()};e.appendChild(x)}
    d.appendChild(e)});
  if(ed.colors.length<3){var p=document.createElement("button");p.className="plus";p.textContent="+";p.title="Add a color";
    p.onclick=function(){ed.colors.push("#FFFFFF");slot=ed.colors.length-1;renderDots()};d.appendChild(p)}
  $("bHex").value=ed.colors[slot];bWheel.set(ed.colors[slot]);
}
$("bHex").onchange=function(){if(validHex(this.value)){ed.colors[slot]=normHex(this.value);renderDots()}else this.value=ed.colors[slot]};
$("bSpeed").oninput=function(){ed.speed=+this.value;$("bSpV").textContent=ed.speed};
$("bBright").oninput=function(){ed.brightness=+this.value;$("bBrV").textContent=ed.brightness+"%"};
$("bEffect").onchange=function(){ed.effect=this.value};
$("bBack").onclick=function(){sheet("v-brand",0)};
$("bPrev").onclick=function(){ed.name=$("bName").value.trim()||"Preview";send("preset",presetCmd(ed))};
$("bSave").onclick=function(){
  ed.name=$("bName").value.trim();if(!ed.name){toast("Give the brand a name.");return}
  var prev=brands.slice();if(editIdx>=0)brands[editIdx]=ed;else brands.push(ed);
  saveBrands().then(function(){sheet("v-brand",0);renderGrid();toast("Saved")}).catch(function(e){brands=prev;toast(e.message)});
};
$("bDel").onclick=function(){
  if(editIdx<0||!confirm("Delete "+brands[editIdx].name+"?"))return;
  var prev=brands.slice();brands.splice(editIdx,1);if(activeBrand===editIdx)activeBrand=-1;
  saveBrands().then(function(){sheet("v-brand",0);renderGrid()}).catch(function(e){brands=prev;toast(e.message)});
};

// ---------- profile ----------
function fillProfile(){
  $("pUser").textContent="Signed in as "+USER;
  if(info){$("pAp").textContent="Device AP: "+info.ap_ssid+" ("+info.ap_ip+")";$("pSite").textContent="Wi-Fi: "+siteText(info);
    $("pIp").textContent="IP address: "+(info.sta_ip||info.ap_ip);if(!$("pSsid").value)$("pSsid").value=info.sta_ssid;if(!$("pApS").value)$("pApS").value=info.ap_ssid}
}
$("avBtn").onclick=function(){loadInfo().then(fillProfile);$("pNewU").value=USER;sheet("v-prof",1);window.scrollTo(0,0)};
$("pBack").onclick=function(){sheet("v-prof",0)};
$("pProv").onclick=function(){provision($("pSsid").value.trim(),$("pWpass").value,{set textContent(m){if(m)toast(m)}},this)};
function backToSetup(msg){T=null;sheet("v-prof",0);sheet("v-brand",0);show("setup");loadInfo();toast(msg)}
$("pAcc").onclick=function(){
  var u=$("pNewU").value.trim(),p=$("pNewP").value;
  if(u.length<3){toast("Username needs 3 or more characters.");return}
  if(p.length<6){toast("Password needs 6 or more characters.");return}
  post("/api/account",{old:$("pOld").value,user:u,pass:p}).then(function(){$("user").value=u;backToSetup("Login changed. Sign in again.")}).catch(function(e){toast(e.message)});
};
$("pApBtn").onclick=function(){
  var s=$("pApS").value.trim(),p=$("pApP").value;
  if(!s||s.length>32){toast("AP name needs 1 to 32 characters.");return}
  if(p.length<8){toast("AP password needs 8 or more characters.");return}
  if(!confirm("The device restarts as \""+s+"\". Join it again, then sign in."))return;
  post("/api/wifi",{ssid:s,pass:p}).then(function(){backToSetup("Device restarting as "+s+".")}).catch(function(e){toast(e.message)});
};
$("pOut").onclick=function(){post("/api/logout").catch(function(){}).then(function(){backToSetup("Signed out.")})};

// ---------- start ----------
loadInfo();
setInterval(function(){if($("v-setup").className.indexOf("on")>=0)loadInfo()},4000);
})();
</script>
</body>
</html>
)MORAG_HTML";

