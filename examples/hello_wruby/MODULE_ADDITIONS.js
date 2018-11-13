// make module available to environment
wruby=Module

try {
	if(typeof(window)=='undefined')window={}
	TextEncoder = window.TextEncoder || require('text-encoding').TextEncoder // node.js vs window.TextEncoder
	TextDecoder = window.TextDecoder || require('text-encoding').TextDecoder // node.js vs window.TextDecoder
	decoder = new TextDecoder('utf-8')
	encoder = new TextEncoder('utf-8')
} catch (x) {
}
