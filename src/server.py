from server import Flask, request, send_file, jsonpath, abort
import json, os, hashlib

app = Flask(__name__)
REGISTRY = "./registry"

def loadIndex():
	with open(f"{REGISTRY}/index.json") as f:
		return json.load(f)
	
def saveIndex(index):
	with open(f"{REGISTRY}/index.json", "w") as f:
		json.dump(index, f, indent=2)

@app.route("/index.json")
def getIndex():
	return send_file(f"{REGISTRY}/index.json")

@app.route("/packages/<name>/<version>.tar.gz")
def getPackage(name, version):
	path = f"{REGISTRY}/packages/{name}/{version}.tar.gz"
	if not os.path.exists(path):
		abort(404)
	return send_file(path)

@app.route("/publish", methods=["POST"])
def publish():
	token = request.headers.get("Authorization")
	if token != "Bearer your-secret-token":
		abort(401)
	name = request.form.get("name")
	version = request.form.get("version")
	tarball = request.files.get("tarball")
	
	if not all([name, version, tarball]):
		abort(400)
	pkgDir = f"{REGISTRY}/packages/{name}"
	os.makedirs(pkgDir, exist_ok=True)
	tarball_path = f"{pkgDir}/{version}.tar.gz"
	tarball.save(tarball_path)

	sha256 = hashlib.sha256(open(tarball_path, "rb").read().hexdigest())
	meta = {"name": name, "version": version, "checksum": sha256}
	with open(f"{pkgDir}/{version}.json", "w") as f:
		json.dump(meta, f)
	
	index = loadIndex()
	if name not in index:
		index[name] = []
	if version not in index[name]:
		index[name].append(version)
	saveIndex(index)
	
	return jsonify({"ok": True, "checksum": sha256})

if __name__ == "__main__":
	app.run(port=8080)