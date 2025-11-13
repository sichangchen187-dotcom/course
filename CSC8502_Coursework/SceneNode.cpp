#include "SceneNode.h"
#include "../nclgl/Camera.h"
#include "../nclgl/MeshMaterial.h"

namespace {
	constexpr const float NODE_REACH_THRESHOLD = 5.f;
}

SceneNode::SceneNode(Mesh* mesh, Vector4 colour) {
	_mesh = mesh;
	_colour = colour;
	_parent = nullptr;
	_modelScale = Vector3(1, 1, 1);
	_texture = 0;
}

SceneNode::~SceneNode() {
	for (int i = 0; i < _children.size(); i++) {
		delete _children[i];
	}
}

void SceneNode::setTransform(const Matrix4& matrix) {
	_transform = matrix;
}

const Matrix4& SceneNode::getTransform() const {
	return _transform;
}

Matrix4 SceneNode::getWorldTransform() const {
	return _worldTransform;
}

Vector4 SceneNode::getColour() const {
	return _colour;
}

void SceneNode::setColour(Vector4 colour) {
	_colour = colour;
}

Vector3 SceneNode::getModelScale() const {
	return _modelScale;
}

void SceneNode::setModelScale(Vector3 modelScale) {
	_modelScale = modelScale;
}

void SceneNode::setScale(float scale) {
	Vector3 newScale = Vector3(_modelScale.x * scale, _modelScale.y * scale, _modelScale.z * scale);
	setModelScale(newScale);
	for (SceneNode* child : _children) {
		child->setScale(scale);
	}
}

Mesh* SceneNode::getMesh() const {
	return _mesh;
}

void SceneNode::setMesh(Mesh* mesh) {
	_mesh = mesh;
}

float SceneNode::getBoundingRadius() const {
	return _boundingRadius;
}

void SceneNode::setBoundingRadius(float boundingRadius) {
	_boundingRadius = boundingRadius;
}

float SceneNode::getCameraDistance() const {
	return _distanceFromCamera;
}

void SceneNode::setCameraDistance(float distanceFromCamera) {
	_distanceFromCamera = distanceFromCamera;
}

Shader* SceneNode::getShader() const {
	return _shader;
}

void SceneNode::setShader(Shader* shader) {
	_shader = shader;
	for (SceneNode* childNode : _children)
	{
		childNode->setShader(shader);
	}
}

std::string& SceneNode::getNodeName() {
	return _nodeName;
}

void SceneNode::setNodeName(const std::string& nodeName) {
	_nodeName = nodeName;
}

bool SceneNode::getIsAnimated() const {
	return _isAnimatedNode;
}

void SceneNode::setIsAnimated(bool isFrustrumCheckable) {
	_isAnimatedNode = isFrustrumCheckable;
}

GLuint SceneNode::getTexture() const {
	return _texture;
}

void SceneNode::setTexture(GLuint texture) {
	_texture = texture;
}

MeshMaterial* SceneNode::getMeshMaterial()
{
	return _meshMaterial;
}

void SceneNode::setMeshMaterial(MeshMaterial* meshMaterial){
	_meshMaterial = meshMaterial;
	for (int i = 0; i < _mesh->GetSubMeshCount(); ++i) {
		const MeshMaterialEntry* matEntry = _meshMaterial->GetMaterialForLayer(i);

		const string* fileName = nullptr;
		matEntry->GetEntry("Diffuse", &fileName);
		string path = TEXTUREDIR + *fileName;
		GLuint texID = SOIL_load_OGL_texture(path.c_str(), SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS | SOIL_FLAG_INVERT_Y);
		_materialTextures.emplace_back(texID);
	}
}

GLuint SceneNode::getShadowTexture() const {
	return _shadowTexture;
}

void SceneNode::setShadowTexture(GLuint shadowTexture) {
	_shadowTexture = shadowTexture;
}

GLuint SceneNode::getBumpTexture() const {
	return _bumpTexture;
}

void SceneNode::setBumpTexture(GLuint bumpTexture) {
	_bumpTexture = bumpTexture;
}

bool SceneNode::compareByCameraDistance(SceneNode* firstNode, SceneNode* secondNode) {
	if (firstNode->getIsAnimated())
		return true;
	else if (secondNode->getIsAnimated())
		return false;
	return firstNode->_distanceFromCamera < secondNode->_distanceFromCamera ? true : false;
}

void SceneNode::addChild(SceneNode* child) {
	_children.push_back(child);
	child->_parent = this;
}

void SceneNode::removeChild(SceneNode* child) {
	for (int i = 0; i < _children.size(); i++) {
		if (_children[i] == child) {
			_children.erase(_children.begin() + i);
			child->_parent = nullptr;  
			return;
		}
	}
}

Light* SceneNode::getLight() {
	return _light;
}

void SceneNode::setLight(Light* light) {
	_light = light;
}

Camera* SceneNode::getCamera()
{
	return _camera;
}

void SceneNode::setCamera(Camera* camera) {
	_camera = camera;
}

void SceneNode::update(float dt) {
	if (_parent)
		_worldTransform = _parent->_worldTransform * _transform;
	else
		_worldTransform = _transform;

	if (_isMoveable && _nodesToVisit.size() != 0){
		handleMovement(dt);
	}

	for (vector<SceneNode*>::iterator i = _children.begin(); i != _children.end(); ++i) {
		(*i)->update(dt);
	}
}

void SceneNode::draw(OGLRenderer& renderer, bool isDrawingForShadows) {
	if (isDrawingForShadows) {
		_mesh->Draw();
		return;
	}

	if (_shader) {
		setUpShader(renderer);
	}
	if (_materialTextures.size() != 0 && _mesh)
	{
		for (int i = 0; i < _mesh->GetSubMeshCount(); i++) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, _materialTextures[i]);
			_mesh->DrawSubMesh(i);
		}
	}
	else
	{
		if (_mesh)
			_mesh->Draw();
	}


	postDraw(renderer);
}

void SceneNode::initMovement(std::vector<Vector3> nodesToVisit, bool isLoopable, float speed)
{
	_isMoveable = true;
	_isLoopingBetweenNodes = isLoopable;
	_nodesToVisit = nodesToVisit;
	_speed = speed;
	_currentNodeToVisit = nodesToVisit[0];
}

std::vector<SceneNode*>::const_iterator SceneNode::getChildIteratorStart()
{
	return _children.begin();
}

std::vector<SceneNode*>::const_iterator SceneNode::getChildIteratorEnd()
{
	return _children.end();
}

void SceneNode::setUpShader(OGLRenderer& renderer) {

	Matrix4 model = getWorldTransform() * Matrix4::Scale(getModelScale());

	glUniformMatrix4fv(glGetUniformLocation(_shader->GetProgram(), "modelMatrix"), 1, false, model.values);

	glUniform4fv(glGetUniformLocation(_shader->GetProgram(), "nodeColour"), 1, (float*)&getColour());

	if (_texture)
	{
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, _texture);
	}

	if (!_bumpTexture)
	{

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, _bumpTexture);
	}

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, _shadowTexture);

	renderer.setModelMatrix(model);
	renderer.UpdateShaderMatrices();

}

void SceneNode::postDraw(OGLRenderer& renderer)
{
}

void SceneNode::handleMovement(float dt){
 	float currentSpeed = _speed * dt;
	Vector3 currentPosition = _transform.GetPositionVector();
	float distanceToNode = (_currentNodeToVisit - currentPosition).Length();
	bool isArrivedToNextNode = distanceToNode < NODE_REACH_THRESHOLD;
	if (isArrivedToNextNode) {
		_completedNodes.push_back(_currentNodeToVisit);
		_currentNodeIndex++;

		if (_nodesToVisit.size() == _currentNodeIndex) {
			if (!_isLoopingBetweenNodes){
				_isMoveable = false;
				return;
			}
			_nodesToVisit = _completedNodes;
			_completedNodes.clear();
			_currentNodeIndex = 0;
		}

		_currentNodeToVisit = _nodesToVisit[_currentNodeIndex];
	}

	Vector3 direction = (_currentNodeToVisit - currentPosition).Normalised();
	moveTowards(currentSpeed, direction, currentPosition);
}

void SceneNode::moveTowards(float currentSpeed, Vector3 direction, Vector3 currentPosition){
	//lookAt(direction);

	currentPosition += direction * currentSpeed;
	setTransform(Matrix4::Translation(currentPosition));
}
