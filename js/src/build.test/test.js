var N = 100000;
var i;
var out = new Array();


//var start = new Date().getTime();

for (i = 0; i < N; i+=5)
{
	out[i] = ((i+i+2)*2)/3;
	//out[i] = i+1;
}

//var end = new Date().getTime() - start;
//print(" " + end);

print("N = " + N + ", out[N-1] = " + out[N-1]);
