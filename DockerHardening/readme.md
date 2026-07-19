# Hardening a Docker container on Windows running Docker Desktop

To learn a thing or two about docker, I decided to set up a docker image, containing a deliberately vulnerable application and see how various security settings impact various attacks. For this set-up I used a windows host system running with docker desktop and used python flask for the web application. In this write-up, I cover the process of setting up a simple container with our web application that deliberately contains Remote Code Execution (RCE) through a system command, obtained via the URL.

Some security controls will be a little bit redundant. For example having a read only file system already makes it impossible to install malicious software as an attacker, however by stripping the image of pip, apt and other package managers, it makes it even harder for an attacker. For the sake of demonstration and of course the defense in depth principle, I'll apply both controls :). The controls selected implement these principles:

- **Assume breach** (we assume an attacker got in via the application RCE);
- **Reduce attack surface** (the attacker inside the container should have the minimal amount of additional pivoting points)
- **Reduce blast radius** (the attack should be confined to the container, not being able to impact the host, other containers or other systems)
- **Defense in depth** (we don't rely on a single control)
- **Principle of the least privilege** (No service or user should operate with more privileges than strictly required to perform its intended task)

## 1. Initial setup

Create hello container world `app.py`

```python
from flask import Flask, request
import subprocess

app = Flask(__name__)

@app.route("/")
def home():
    return "Hello, container world!"

@app.route("/run", methods=["GET"])
def run_cmd():
    cmd = request.args.get("cmd")

    if not cmd:
        return "No command provided", 400

    try:
        # Run command and capture output
        result = subprocess.check_output(cmd, shell=True, stderr=subprocess.STDOUT)
        return f"Command: {cmd}\n\nOutput:\n{result.decode()}"
    except subprocess.CalledProcessError as e:
        return f"Command failed:\n{e.output.decode()}", 500

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=1337)
```

Create `requirements.txt`

```text
Flask
flask-cors
Flask-OAuth
Flask-JWT-Extended
flask-limiter
Flask-Session
```

Create `Dockerfile` (make sure it doesn't have an extension!)

```dockerfile
# Use an official Python runtime as a parent image
FROM python:3.9-slim-buster

# Set the working directory to /app
WORKDIR /app

# Copy the current directory contents into the container at /app
COPY . /app

# Install any needed packages specified in requirements.txt
RUN pip install -r requirements.txt

# Make port 1337 available to the world outside this container
EXPOSE 1337

# Define environment variable
ENV FLASK_APP=app.py

# Run app.py when the container launches
CMD [ "python", "-m" , "flask", "run", "--host=0.0.0.0", "--port=1337"]
```

> **Note:** `flask run` is singlethreaded, not hardened, not optimized, not stable under load, not secure, and not meant for internetfacing traffic. I'm using it in this write-up purely for demonstration purposes!

Start docker desktop.

Build the image:

```bash
docker build -t myapp .
```

Run the app:

```bash
docker run -p 1337:1337 myapp
```

Execute a command:

```bash
http://localhost:1337/run?cmd=whoami
```

## 2. Security evaluation & hardening measures

### 2.0. Threat model of the application

For any security evaluation it's good practice to document your security assumptions and threat model upfront. In general we have limited time that we want to spend on something (even though, some companies might spend months on threat modeling), realistically there is a limit to what can and can't be covered. The main purpose of this walkthrough is to gain a basic understanding of docker security controls and validate its working against a practical attack.

We assume an attacker scenario, where a docker application (python) is exposed to the external world. The application has a Remote Code Execution vulnerability and as such the attacker has full arbitrary command execution inside the container. The attacker does not have any credentials or direct access to the docker daemon. By hardening the container we want to establish the following security objectives:

- **#1 Objective:** avoid that an attacker can abuse the RCE to make a significant impact on the host system;
- **#2 Objective:** ensure any RCE only results in getting a minimal foothold to the container
- **#3 Objective:** ensure the attacker cannot compromise secrets or a minimal amount of secrets
- **#4 Objective:** avoid letting an attacker abuse the container resources for malicious purposes such as
  - **4a:** installing crypto miners and making a profit while a financial impact is presented to the company hosting the container
  - **4b:** use the container as an attacker proxy, attacker platform, to attack other systems
  - **4c:** use the container to host criminal content (stolen data, host a C2, etc.)
- **#5 Objective:** avoid letting the RCE have an impact on other users
  - **5a:** accessing user PII
  - **5b:** injecting malware on the front-page that users might download as they trust the website

To simplify the hardening and attack examples, I assume no kernel exploit is currently available (I will address the security of the host platform in another write-up) and assume the attacker cannot modify the image. Although image signing is addressed as a hardening control, for the sake of time, I'm not taking supply chain attacks (package managers and image repositories) into account. For actual applications, you should!

### 2.1. Apply the principle of the least privilege

The application is currently running as the root user, by visiting: `http://localhost:1337/run?cmd=whoami` you will observe:

![whoami shows the application is running as root](./img/whoamiroot.png)

This is not a good practice. So let's add a group and a user to the Dockerfile and run the app with lower user privileges.

```dockerfile
# Use an official Python runtime as a parent image
FROM python:3.9-slim-buster

# Set the working directory to /app
WORKDIR /app

# Add a group
RUN groupadd -r appgroup && useradd -r -g appgroup appuser

# Copy the current directory contents into the container at /app
COPY . /app

# Install any needed packages specified in requirements.txt
RUN pip install -r requirements.txt

# Change ownership
RUN chown -R appuser:appgroup /app

# Switch user
USER appuser

# Make port 1337 available to the world outside this container
EXPOSE 1337

# Define environment variable
ENV FLASK_APP=app.py

# Run app.py when the container launches
CMD [ "python", "-m" , "flask", "run", "--host=0.0.0.0", "--port=1337"]
```

![whoami now shows the application is running as appuser](./img/whoamiappuser.png)

### 2.2. Further lock down the container: make the file system immutable

With the current RCE vulnerability, an attacker can modify app.py and inject malicious code, effectively turning the application into a delivery mechanism for targeting other users. It could also use the directory (or other parts of the container) to temporarily store malicious content. In our case the container does not use external mounts for storing the data, and the container is ephemeral (e.g. it's not keeping any modifications made during runtime), regularly rebooting might not make this an ideal place for attackers to store malicious content. Nonetheless, we want to further lock down the container file system, as currently the RCE allows for writing output to the container:

![The RCE can write a file into the container filesystem](./img/03-rce-writes-file-to-container.png)

To prevent this from happening, we run our container with a read only file system:

```bash
docker run -p 1337:1337 --read-only myapp
```

![The read-only filesystem blocks the write](./img/04-read-only-filesystem-blocks-write.png)

However, if you need to write session cookies, or have one directory where you need to write something, you can add one (or more) exception(s):

```bash
docker run -p 1337:1337 --read-only --tmpfs /tmp/flask_session:rw,exec,size=64k myapp
```

![The tmpfs exception allows writing to /tmp/flask_session](./img/05-tmpfs-exception-allows-write.png)

It's important to notice that the `/tmp/flask_session` gets read, write and exec privileges in this scenario. That means that an attacker can upload a malicious file to this directory and execute it as well via the RCE.

![With exec set, an attacker can execute a malicious file from tmpfs](./img/06-tmpfs-exec-allows-malicious-binary.png)

So to further lock down the container and contribute to security objective 4a, we run it with only read and write privileges:

```bash
docker run -p 1337:1337 --read-only --tmpfs /tmp/flask_session:rw,size=64k myapp
```

Denying the attacker from profiting from crypto mining via this directory (or any other directory on the system)

![Without exec, execution from tmpfs is blocked](./img/07-tmpfs-without-exec-blocks-execution.png)

### 2.3. Check the container image for known vulnerabilities

Many applications rely on other software libraries, components or even complete operating systems for their security. Lately, there is a trend of package managers being targeted by adversaries, as this is an effective way to gain access to many applications at once. The security objective in this paragraph is not to solve the supply-chain attack issue, however, we do want to make sure that our container is shipped without known exploitable vulnerabilities. As such we want to perform a vulnerability scan on the container images selected and the libraries that are brought on board of our application. Again, another caveat, we won't go into the details of running tools like the OWASP dependency checker (or others) on your application, as this walkthrough focusses on securing the container (and not the application itself). Securing software is a continuous effort that should be built into your CI/CD pipeline, as part of a secure software development life cycle process.

To scan the container for known vulnerabilities, I used [trivy](https://github.com/aquasecurity/trivy/releases/), a container vulnerability scanning tool. The tool could be executed via:

```bash
trivy image myapp:latest
```

![Trivy scan of the python:3.9-slim-buster based image](./img/08-trivy-scan-python-slim-buster.png)

As you could see from the first run, the application contained 126 vulnerabilities. From here we have multiple options, validate if we have the latest release? In the demo, we deliberately used `python:3.9-slim`. Updating to the latest slim version, still flagged quite some vulnerabilities. So again, we had the choice to either start targeting each vulnerability (assess exploitability, evaluate if there is an update/workaround) or pick another image that has fewer features. In this case I chose the latter and selected Alpine as the new container image. `python-slim` is based on Debian (using glibc) where Alpine is Linux based using musl-libc. That means for our Dockerfile to be able to compile we need to update the RUN command that creates the groups to:

```dockerfile
RUN addgroup -S appgroup && adduser -S -G appgroup appuser
```

![Trivy scan of the Alpine based image](./img/09-trivy-scan-alpine-image.png)

Let's dig a bit deeper and generate a software bill of materials (SBOM) for our container and check for vulnerabilities:

```bash
docker sbom myapp
```

![docker sbom output listing the container packages](./img/10-docker-sbom-output.png)

```bash
docker scout cves myapp
```

![docker scout cves output](./img/11-docker-scout-cves-output.png)

### 2.4. Activate a Secure Computing Mode (SECCOMP) profile

The docker Secure computing mode (SECCOMP) allows you to block specific syscalls, or allow-list specific syscalls (which is even stronger). The default seccomp profile, strips around 44 syscalls out of 300+ available to make the container execution more secure (https://docs.docker.com/engine/security/seccomp/). On the other hand, it's also possible to block syscalls you don't fancy. Let's watch an example and use the Dockerfile from above, and use the RCE to change the permissions of the app.py application

![chmod via the RCE succeeds without a seccomp profile](./img/12-chmod-via-rce-succeeds.png)

As a next step, we can create a `block_chmod.json` file, with the following content:

```json
{
  "defaultAction": "SCMP_ACT_ALLOW",
  "syscalls": [
    { "names": ["chmod", "fchmod", "fchmodat", "fchmodat2"],
      "action": "SCMP_ACT_ERRNO", "errnoRet": 1 }
  ]
}
```

Let's run our container with the following parameter:

```bash
docker run -p 1337:1337 --security-opt seccomp=block_chmod.json myapp
```

![The seccomp profile blocks the chmod syscall](./img/13-seccomp-profile-blocks-chmod.png)

In this case, the operation is not permitted anymore. Not by blocking the capabilities of the container or the permissions inside the container, but by disallowing the syscalls required to change the permissions. To get a bit more visibility into what syscalls are executed, you should start your container (and ensure it has the name myapp):

```bash
docker run -p 1337:1337 --name myapp myapp
```

Afterwards, you could monitor the syscalls made, to build a more minimalized list to really lock down the container:

```bash
docker run --rm --pid=container:myapp --cap-add=SYS_PTRACE --security-opt seccomp=unconfined nicolaka/netshoot strace -f -p 1
```

![strace output showing the syscalls the container makes](./img/14-strace-syscall-monitoring.png)

Where: so `accept4`, `getsockname`, `clone`, `set_robust_list`, `futex`, `getid` are the names of syscalls that are being used by your container (and for it to function, should be contained in the minimal allowed set to function).

### 2.5. Dropping application capabilities

In the previous section, we have demonstrated that the individual syscalls can be blocked to prevent certain operations from being executed. Another way to block actions from happening, is by dropping application capabilities. In this case one of the 40+ privileged capabilities (https://man7.org/linux/man-pages/man7/capabilities.7.html#NAME) is banned for the container and cannot be used. However, as we are demonstrating that you could drop privileged capabilities, we first need to run the container as a privileged user:

```bash
docker run -p 1337:1337 --user=root myapp
```

![chown succeeds when running as root](./img/15-chown-as-root-succeeds.png)

Now, let's drop the CHOWN capability, and try again:

```bash
docker run -p 1337:1337 --user=root --cap-drop=CHOWN myapp
```

![Dropping the CHOWN capability blocks chown even for root](./img/16-cap-drop-chown-blocks-chown.png)

As a result, even the root user cannot make arbitrary changes to UIDs anymore.

### 2.6. no-new-privileges

To demonstrate the power of no-new-privileges we need to switch distros as Alpine works with busybox that doesn't allow for SUIDs. So we switch back to python-slim. First, let's create a file called `suiddemo.c` containing:

```c
#include <stdio.h>
#include <unistd.h>
int main() {
    printf("Real UID: %d\n", getuid());
    printf("Effective UID: %d\n", geteuid());
    return 0;
}
```

Add the following to the Dockerfile

```dockerfile
# Use an official Python runtime as a parent image
FROM python:3.12-slim

# Set the working directory to /app
WORKDIR /app

# Add a group
RUN groupadd -r appgroup && useradd -r -g appgroup appuser

# Copy the current directory contents into the container at /app
COPY . /app

# Build the SUID demo from a real source file (no shell-printf %d corruption)
RUN apt-get update && apt-get install -y gcc && gcc /app/suiddemo.c -o /app/suiddemo

# Install any needed packages specified in requirements.txt
RUN pip install -r requirements.txt

# Change ownership of everything to the unprivileged user
RUN chown -R appuser:appgroup /app

# Make suiddemo root-owned + SUID *after* the recursive chown,
# otherwise chown strips the SUID bit.
RUN chown root:root /app/suiddemo && chmod u+s /app/suiddemo

# Switch user
USER appuser

# Make port 1337 available to the world outside this container
EXPOSE 1337

# Define environment variable
ENV FLASK_APP=app.py

# Run app.py when the container launches
CMD [ "python", "-m" , "flask", "run", "--host=0.0.0.0", "--port=1337"]
```

Now issue the commands:

```bash
docker build -t myapp .
docker run -p 1337:1337 --read-only --tmpfs /tmp/flask_session:rw,size=64k --cap-drop=ALL myapp
```

![The SUID binary reports an effective UID of root](./img/17-suid-demo-effective-uid-root.png)

Now let's enforce that the application cannot gain new privileges by adding no-new-privileges:

```bash
docker run -p 1337:1337 --read-only --tmpfs /tmp/flask_session:rw,size=64k --cap-drop=ALL --security-opt no-new-privileges myapp
```

![no-new-privileges prevents the SUID binary from escalating](./img/18-no-new-privileges-blocks-suid.png)

### 2.7. Use .dockerignore to reduce the risk of information disclosure

With the previous update, we accidentally also added some information disclosure. We copied the `.c` file and security profile into the container, which isn't necessary (unless you are running the setuid demo from above)

![Build files such as the .c source and security profile are visible inside the container](./img/19-build-files-disclosed-in-container.png)

Let's create a `.dockerignore` file containing the following filenames. Please note that the filenames are case sensitive.

```text
myappprofile.json
Dockerfile
suiddemo.c
__pycache__
```

Let's rebuild the application:

```bash
docker build -t myapp .
```

### 2.8. Reduce Denial of service (DoS) risk from container spreading to host

A denial of service (DoS) attack is an attack that makes an application or resource unavailable. An application or resource can become unavailable for only the user that is using it, for all users using it, or worse can propagate to the host system and also cause other services to become less responsive or unavailable. Let's have a look at the normal memory consumption of our container. Note that, as we are running Docker Desktop on a Windows Machine, the container is mounted through the Windows Subsystem for Linux (WSL), which is the windows process we'll be monitoring for performance

![Baseline WSL memory usage for the container](./img/20-wsl-baseline-memory-usage.png)

A way to cause a denial of service via a command prompt (besides issuing a shutdown command :)) is to apply a fork bomb. With a fork bomb a process recursively tries to open multiple instances of itself, growing the number of processes exponentially until the operating system crashes. So by issuing a fork bomb with the current settings, we should be able to cause an impact not only to the container, but also to the host system. After tweaking the fork bomb a bit (encoding!), I found the following command:

```bash
b(){ b|b%26 }%3Bb
```

![The fork bomb consumes host memory through WSL](./img/21-fork-bomb-impacts-host.png)

As a result, a lot of memory of the host system was used and closing the cmd in which docker was running caused some GUI issues. We could prevent this, by giving our container a hard limit on the number of pids to run and bound the memory space of the container as well:

```bash
docker run -p 1337:1337 --read-only --tmpfs /tmp/flask_session:rw,size=64k --cap-drop=ALL --security-opt no-new-privileges --pids-limit=200 --memory=128m --cpus=0.5 myapp
```

![The pids and memory limits contain the fork bomb inside the container](./img/22-pids-and-memory-limits-contain-fork-bomb.png)

As a result, the application is now getting errors, instead of impacting the host system. The problem is contained to the container only. However, issuing other commands after the fork bomb, will also throw you the same error:

![The container stays unresponsive after the fork bomb](./img/23-container-unresponsive-after-fork-bomb.png)

One way to fix this (besides removing the RCE :)), is to work with a HEALTHPROBE system, for example Kubernetes liveness probe, that restarts a container. I'll deep dive into using Kubernetes for security in another write-up. Another parameter to consider, in preventing for example log file flooding is limiting the io operations of the container. This will not be effective on a docker desktop instance, however for a bare metal solution, you should also provide io boundaries using:

```bash
--device-read-bps=/dev/sda:1mb --device-write-bps=/dev/sda:1mb --device-read-iops=/dev/sda:100 --device-write-iops=/dev/sda:100
```

### 2.9. Multistage build & further reducing the attack surface

Although an earlier control is preventing pip from downloading anything to the read-only file system, the capability to install or compile software/code is still available on the system.

![pip install is blocked by the read-only filesystem](./img/24-pip-install-blocked-by-read-only.png)

![The gcc compiler is still available inside the container](./img/25-gcc-compiler-still-available.png)

A good practice in docker is to work with a multi-stage build, meaning that you prepare the app in one container (the build stage) but ship it in a different one. As a result, the shipping container can be even smaller (as build tooling etc. is not necessary in the run container) and by having less tooling available in your container, you are also further reducing the attack surface. Secondly, if your application doesn't rely on certain binaries, there is no need for them to be present in the container. Have a look at https://gtfobins.org/ and decide which ones can be removed. As we are trying to avoid any installations I'm removing `nc`, `wget`, `curl`, `apt`, `pip` to make it even harder to get something. The new Dockerfile contains:

```dockerfile
# ---------- Stage 1: builder (has gcc + pip) ----------

FROM python:3.12-slim AS builder
WORKDIR /build
RUN apt-get update && apt-get install -y --no-install-recommends gcc libc6-dev
RUN rm -rf /var/lib/apt/lists/*
COPY suiddemo.c .
RUN gcc suiddemo.c -o suiddemo
COPY requirements.txt .
RUN pip install --prefix=/install --no-cache-dir -r requirements.txt

# ---------- Stage 2: runtime (NO gcc, NO pip, NO apt) ----------

FROM python:3.12-slim AS runtime
WORKDIR /app
RUN groupadd -r appgroup && useradd -r -g appgroup appuser
COPY --from=builder /install /usr/local
COPY --from=builder /build/suiddemo /app/suiddemo
COPY app.py /app/app.py

RUN rm -rf /usr/local/lib/python3.12/site-packages/pip* /usr/local/lib/python3.12/site-packages/setuptools* /usr/local/lib/python3.12/site-packages/wheel* /usr/local/lib/python3.12/ensurepip /usr/local/bin/pip*

RUN rm -rf /usr/bin/apt* /usr/lib/apt /etc/apt /usr/bin/dpkg* /var/lib/dpkg /var/lib/apt /var/cache/apt

RUN for t in gcc cc apt apt-get apt-cache dpkg dpkg-deb curl wget nc; do rm -f "$(command -v $t 2>/dev/null)" || true; done

RUN chown -R appuser:appgroup /app
RUN chown root:root /app/suiddemo && chmod u+s /app/suiddemo

USER appuser
EXPOSE 1337
ENV FLASK_APP=app.py
CMD [ "python", "-m" , "flask", "run", "--host=0.0.0.0", "--port=1337"]
```

As a result the earlier applications that help with installing or downloading are not available anymore:

![The install and download tooling is gone after the multistage build](./img/26-multistage-build-removes-tooling.png)

### 2.10. Apply Network restrictions to reduce outbound attacks

For the sake of demonstration purposes we remove curl from the forbidden list of the abovementioned Dockerfile and explicitly make sure it's available by installing it:

```dockerfile
RUN apt-get update && apt-get install -y --no-install-recommends curl
```

The application can now generate traffic towards other systems, which following security objective 4b, is something we want to avoid:

![curl inside the container generates outbound traffic](./img/27-curl-generates-outbound-traffic.png)

To block all outgoing traffic, you could use the `--network none` parameters

```bash
docker run -p 1337:1337 --read-only --network none myapp
```

With `--network none` you'll notice the Flask application itself is no longer reachable either — cutting all networking also cuts the published port. Ultimately it's better to enforce container inbound/outbound rules with a real firewall, e.g. iptables or a Kubernetes NetworkPolicy — but managing anything outside the container I'll keep for another write-up. To demonstrate egress filtering at the container level, I placed the app in a separate, isolated network with no route to the outside. So I could still reach Flask from localhost, I added a second network and a small proxy that forwards only port 1337 to the app. The isolated network blocks all outbound traffic; the proxy restores just the inbound path:

```bash
docker network create --internal no-internet
docker network create edge-net
docker run -d --name demo-myapp --network no-internet myapp
docker run -d --name demo-proxy --network edge-net -p 127.0.0.1:1337:1337 alpine/socat tcp-listen:1337,fork,reuseaddr tcp-connect:demo-myapp:1337
docker network connect no-internet demo-proxy
```

![The isolated network with a socat proxy restoring only the inbound path](./img/28-isolated-network-with-socat-proxy.png)

### 2.11. Secrets management

Every application requires some sort of secret at some point. Maybe you want to have access to a database and need to store a connectivity string, maybe you need to store a (hash of) a password to ensure a user can log in, or maybe you want to make a call to a back-end API that requires a secret.

Let's introduce two examples:

1. have a connection string in a config file (added: to Dockerfile using: `COPY config.py /app/config.py`)
2. have a secret as part of the environment variables (which will be injected during runtime, not stored via the `.env`)

```bash
docker build -t myapp .
docker run -p 1337:1337 -e MY_SECRET_ACCESS_KEY=wJalrDEMOfake/K7MDENGbPxRfiCYFAKEKEY --name myapp myapp
```

![The attacker can read both the config file secret and the environment variable](./img/29-secrets-exposed-in-config-and-env.png)

As you can see, the attacker can access both. However, is this really avoidable? Well partially. Let's first move our secrets to a keyvault and then reflect on the problem being solved. For this demo, I picked hashicorpvault as our keyvault. A deep dive into key management solutions and how they can be managed as part of the broader environment will be addressed in another write-up. To demonstrate the improvement the keyvault makes, we need to slightly update our `app.py` and `config.py`

#### Fetch and run hashicorp vault

First let's make a keyvault available for our application, put it on an isolated network and add some credentials to it:

```bash
docker pull hashicorp/vault

docker run -d --name vault --network no-internet --cap-add=IPC_LOCK -e VAULT_DEV_ROOT_TOKEN_ID=root -e VAULT_ADDR=http://127.0.0.1:8200 -e VAULT_TOKEN=root hashicorp/vault server -dev
```

> **Note:** for the sake of this demonstration I'm using devmode which pre-mounts a kv v2 store. In production, you need to initialize this first.
>
> **Note 2:** this solution uses the proxy from the previous section.

#### Put secret in vault and confirm it worked

```bash
docker exec -it vault sh

vault kv put secret/myapp database_url="postgresql://appuser:P%40ss1@db-prod-1.internal:5432/appdb?sslmode=require"

vault kv get secret/myapp
```

#### Update application

Although this walkthrough is not about software security, we do need to make some changes to the software to showcase the effectiveness of using a keyvault/secrets manager. First we add a function that fetches the database credentials to `config.py` and make a routine available in `app.py` that can be called and measured. For `app.py`, we add the following line and routine:

```python
import config

@app.route("/dbinfo")
def dbinfo():
    try:
        return f"DB connection string (live from Vault): {config.get_database_url()}"
    except Exception as e:
        return f"Could not load secret: {e}", 500
```

For `config.py` we need to remove all the static key entries and update the code as a whole:

```python
import os
import json
import urllib.request

VAULT_ADDR = os.environ.get("VAULT_ADDR", "http://vault:8200")
VAULT_SECRET_PATH = "/v1/secret/data/myapp"

def get_database_url():
    """Fetch the connection string from Vault at call time. The secret is NEVER written to this file or to disk."""
    token = os.environ.get("VAULT_TOKEN", "")
    req = urllib.request.Request(VAULT_ADDR + VAULT_SECRET_PATH, headers={"X-Vault-Token": token},)
    with urllib.request.urlopen(req, timeout=3) as resp:
        payload = json.load(resp)
        return payload["data"]["data"]["database_url"]
```

Finally, don't forget to rebuild everything:

```bash
docker build -t myapp .
```

#### Generate least-privilege token, to only access the connection string

Now that we have a keyvault, we need to create a policy for the secret of our app and generate the token that can be used by our application:

```bash
echo 'path "secret/data/myapp" { capabilities = ["read"] }' | vault policy write app-policy -
```

```bash
$ vault token create -policy=app-policy -ttl=2m -field=token
hvs.CAESIII4D6tVHAwas7hpc_CnQ1WyPYr_kzES3OKLyjH5dvy1Gh4KHGh2cy5wVm93blZ6YllKWHdwQTFPaXVXMnBWdWQ
```

#### Run application and inject vault token

Finally, we need to generate the runtime token and inject it to our container:

```bash
docker exec vault vault token create -policy=app-policy -ttl=2m -field=token
hvs.CAESIHGbYt-GffpGImStajJRiktuzMeC1Iv0vMygjSkwp8SPGh4KHGh2cy5jVHVxZmdmbno5OUh1ejlIOGpUYmxSSG4
```

```bash
docker run -d --name demo-myapp --network no-internet -e VAULT_ADDR=http://vault:8200 -e VAULT_TOKEN=hvs.CAESIL6NmQOL3Vofzqp95Qx0LpZUJWYgCHGZMVhGsT54PI1sGh4KHGh2cy5mQmdHT3BGM2FEcUxkMGloUGJrR2N4bDI myapp
```

![The application fetches the database connection string from Vault at runtime](./img/30-vault-dbinfo-fetched-at-runtime.png)

An attacker could still fetch the keyvault short living token from the environment variables and abuse any network connectivity of the application, to fetch the database credentials:

![The attacker uses the leaked Vault token to fetch the database credentials](./img/31-attacker-fetches-secret-with-vault-token.png)

So, what problem did we actually resolve by introducing a keyvault? Clearly, the attacker can still perform a hit and run to our database, which is caused by the RCE of the application. The token rotates too slowly to avoid an attacker dumping the data and taking a run. However, it does positively impact a couple of things:

1. Instead of reading the config file and having the database access, we let the attacker do a bit more work (although, from reading the code, this should be obvious to an attacker, it does take some time to implement and figure out the next step in an attack chain).
2. A leaked static DB password often unlocks other systems. Having a credential vault in place, lets you deliberately think about setting these credentials and manage credential length and complexity with policies. With rotation, complexity and re-use policies in place, the chances of re-using the database credential for other components is reduced, and hence the blast radius of this exposure is reduced as well.
3. Nothing gets into the image layers of our container, limiting the attack surface (although, this was not an objective of this write-up, it's a nice bonus).
4. Detection opportunity, while detecting an unauthorized read of a config file can be done, having a central place to monitor for all credential access makes it easier to detect abuse.
5. Recoverability: The keyvault lowers the effort required for recovery. After fixing the RCE, the owner of the application doesn't have to go through the code and repositories to change the credential of the config file. It can simply be rotated via the keyvault. Especially when multiple applications make use of the same connection string, you reduce the pain of recoverability.

The keyvault in this particular threat model bears an interesting security trade-off. On the one hand we have the opportunity to fully lockdown all capabilities for remote communication with the image. By introducing the keyvault, we do need the application to have a capability to connect outwards. Of course, we can confine this to the no-internet network, but there is a trade-off in ensuring the container cannot be abused by an attacker to connect out (enforced by not having the capability to do so and strict egress network limitations) and storing the keys more securely.

Final note, for Docker swarm services, docker secrets are available (https://docs.docker.com/engine/swarm/secrets/) docker secrets help you manage secrets at runtime and avoid letting them into the image or source control. However, as we are analyzing security controls for a standalone container, we did not go into depth for this one.

### 2.12. Custom log rules

By now, we have addressed the majority of the security objectives defined in our threat model. Throughout this walkthrough, it became clear that several controls have inherent limitations: once the application is vulnerable to Remote Code Execution, an attacker can reach the database (objective 5a) and retrieve the secrets the application must store to function (objective 3). These constraints are structural rather than accidental — an RCE collapses the trust boundary. Fortunately, we still have one powerful control left: logging and security monitoring. This final layer does not prevent the attacker from reaching sensitive components, but it does allow us to detect, contain, and respond to malicious activity before the impact escalates. Docker logs can be viewed via:

```bash
docker logs vault
```

However, they contain a lot of generic logs as well. Luckily there is a solution that focusses on the security logging. Falco is an open-source security tool that monitors the security of docker containers. Let's install and run the solution:

```bash
docker pull falcosecurity/falco:latest
docker run --rm -it --name falco --privileged -v /var/run/docker.sock:/host/var/run/docker.sock -v /proc:/host/proc:ro falcosecurity/falco:latest falco
```

Great, falco is now up and running, let's trigger a warning by using: `http://127.0.0.1:1337/run?cmd=find%20/%20-name%20id_rsa`

![Falco raises an alert on the search for id_rsa](./img/32-falco-alerts-on-id-rsa-search.png)

Great, now let's add a custom ruleset! To do so, create a file called `falco_extra_rules.yaml` and append the following:

```yaml
- rule: Canary command DETECTMEPLEASE executed
  desc: Fires when the DETECTMEPLEASE canary string runs in a container (RCE tripwire demonstration)
  condition: >
    spawned_process and container
    and proc.cmdline contains "DETECTMEPLEASE"
  output: >
    CANARY HIT: DETECTMEPLEASE executed
    (container=%container.name cmdline=%proc.cmdline parent=%proc.pname user=%user.name uid=%user.uid)
  priority: CRITICAL
  tags: [rce, canary]
```

When running the falcosecurity container with the additional yaml files, it will append it to the current configuration. So no need to redo the whole file.

```bash
docker run --rm -it --name falco --privileged -v /var/run/docker.sock:/host/var/run/docker.sock -v /proc:/host/proc:ro -v "C:\Users\testuser\dockertest\falco_extra_rules.yaml:/etc/falco/rules.d/webapp_rules.yaml:ro" falcosecurity/falco:latest
```

![The custom canary rule fires in Falco](./img/33-falco-custom-canary-rule-fires.png)

Perfect, we can now create custom detection rules for our container image. More about creating custom rules can be found on: https://falco.org/docs/concepts/rules/custom-ruleset/. Based on the security trade-off you had to make while configuring the security of this application, you can plug the gaps. For example, if you decided to work with the secrets manager and have curl still available in your container, you can now monitor that curl is not being abused to get the secret to the attacker. Or maybe even broader, that the output doesn't contain the format of the secrets string.

Having log warnings is great, but to increase your security, you also need to be able to act on them. Of course you can monitor the log file yourself, but this is not very practical. Luckily we can extend the falcosecurity module and let it send the logs to the SIEM of your choice. As this walkthrough is not about setting up SIEM or SOAR, or installing anything specific (Sentinel, Splunk etc.), claude just created this `siem.py` stub for the sake of the demonstration:

```python
from http.server import BaseHTTPRequestHandler, HTTPServer

class SIEM(BaseHTTPRequestHandler):
    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", 0)))
        print("=== ALERT RECEIVED FROM FALCO ===")
        print(body.decode(errors="replace"), flush=True)
        self.send_response(200)
        self.end_headers()

HTTPServer(("0.0.0.0", 8000), SIEM).serve_forever()
```

Now, let's run falco with SIEM enabled

```bash
docker run --rm -it --name falco --privileged -v /var/run/docker.sock:/host/var/run/docker.sock -v /proc:/host/proc:ro -v "C:\Users\testuser\dockertest\falco_extra_rules.yaml:/etc/falco/rules.d/webapp_rules.yaml:ro" falcosecurity/falco:latest falco -o json_output=true -o http_output.enabled=true -o "http_output.url=http://192.168.2.10:8000/"
```

![The Falco alert is forwarded to the SIEM stub](./img/34-falco-alert-forwarded-to-siem.png)

This example shows a container can be connected to a SIEM. The solution might not scale well, but connecting a Kubernetes cluster to a SIEM, will be discussed in a later write-up.

## Final remarks

The above write-up mainly covers the security aspects of the docker container itself. Obviously there are many more aspects that need to be designed well for a secure solution. For example the code itself needs to be secure, the CI/CD needs to validate and integrate security. Container images require image signing, and also the host system requires protection. Some of the solutions don't scale well. However, I hope this write-up demonstrated the practical impact of some of the available security settings!
