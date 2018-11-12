window.prompt=function(){
result.innerText="please Type"
return null
}
evaluate=function (result){
            Module.tty.input = intArrayFromString(result, true);
            return Module.tty.input.shift();
}