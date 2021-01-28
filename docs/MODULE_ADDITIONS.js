// make module available to environment
wruby=Module
main=wruby.callMain

/*
d=decode
main()
start=221776
v=new Uint8Array(wruby.buffer,start,400)
bak=wruby.buffer.slice(start,start+400)
v.set(bak,0,bak.length)
main()

p=read_raw('test.mrb')
v=new Uint8Array(wruby.buffer,start,p.length)
v.set(p,0,p.length)
main()
*/

try {
	TextEncoder = require('text-encoding').TextEncoder // node.js vs window.TextEncoder
	TextDecoder = require('text-encoding').TextDecoder // node.js vs window.TextDecoder
	decoder = new TextDecoder('utf-8')
	encoder = new TextEncoder('utf-8')
} catch (x) {
}
