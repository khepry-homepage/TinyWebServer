const o=axios.create({method:"post",headers:{"Content-Type":"application/x-www-form-urlencoded"},transformRequest:[function(e){let r="",n=Object.keys(e);for(let t=0;t<n.length;++t)r+=encodeURIComponent(n[t])+"="+encodeURIComponent(e[n[t]]),t<n.length-1&&(r+="&");return r}],timeout:5e3}),s={login:"/login",register:"/register"};function a({loginForm:e}){return o({url:s.login,data:{username:e.username,password:e.password}})}function i({registerForm:e}){return o({url:s.register,data:{username:e.username,password:e.password}})}const u={login:a,register:i};export{u as a};