#include "Renderer.h"
#include "../nclgl/Camera.h"
#include "CubeRobot.h"
#include "SkyboxNode.h"
#include <algorithm>
#include <map>
#include "SkyboxNode.h"
#include "SpaceObject.h"
#include "HeightmapNode.h"
#include "../nclgl/Light.h"
#include "AnimatedSceneNode.h"
#include "../nclgl/MeshAnimation.h"
#include "../nclgl/MeshMaterial.h"
#include "../nclgl/HeightMap.h"
#include "ParticleNode.h"

namespace {

	constexpr const int SHADOW_SIZE = 2048;
	constexpr const int POST_PASSES = 10;

	const std::vector<std::string> planetCubeMapPaths = {
	TEXTUREDIR"posx.jpg", // +X (right)
	TEXTUREDIR"negx.jpg", // -X (left)
	TEXTUREDIR"posy.jpg", // +Y (top)
	TEXTUREDIR"negy.jpg", // -Y (bottom)
	TEXTUREDIR"posz.jpg", // +Z (front)
	TEXTUREDIR"negz.jpg"  // -Z (back)
	};

	constexpr const char* SPHERE_MESH_FILE_NAME = "Sphere.msh";
	constexpr const char* TREE_MESH_FILE_NAME = "Tree.msh";
	constexpr const char* ANIMATED_CHARACTER_MESH_FILE_NAME = "Role_T.msh";

	constexpr const char* ANIMATED_CHARACTER_ANIMATION_FILE_NAME = "Role_T.anm";

	constexpr const char* ANIMATED_CHARACTER_MATERIAL_FILE_NAME = "Role_T.mat";
	constexpr const char* NOISE_MAP_FILE_NAME = "islandNoise.jpg";

	constexpr const char* SCENE_SHADER_VERTEX_FILE_NAME = "SceneVertex.glsl";
	constexpr const char* SCENE_SHADER_FRAGMENT_FILE_NAME = "SceneFragment.glsl";

	constexpr const char* SCENE_SHADER_FRAGMENT_WITH_SHADOWS = "PerPixelFragment.glsl";
	constexpr const char* SCENE_SHADER_VERTEX_WITH_SHADOWS = "PerPixelVertex.glsl";

	constexpr const char* SKYBOX_SHADER_VERTEX_FILE_NAME = "skyboxVertex.glsl";
	constexpr const char* SKYBOX_SHADER_FRAGMENT_FILE_NAME = "skyboxFragment.glsl";

	constexpr const char* ANIMATED_SHADER_VERTEX_FILE_NAME = "SkinningVertex.glsl";
	constexpr const char* ANIMATED_SHADER_FRAGMENT_FILE_NAME = "TexturedFragment.glsl";

	constexpr const char* POST_PROCESS_SHADER_VERTEX_FILE_NAME = "TexturedVertex.glsl";
	constexpr const char* POST_PROCESS_SHADER_FRAGMENT_FILE_NAME = "processfrag.glsl";

	constexpr const char* PRESENT_SHADER_VERTEX_FILE_NAME = "TexturedVertex.glsl";
	constexpr const char* PRESENT_SHADER_FRAGMENT_FILE_NAME = "TexturedFragment.glsl";

	constexpr const char* REFLECT_SHADER_VERTEX_FILE_NAME = "reflectVertex.glsl";
	constexpr const char* REFLECT_SHADER_VERTEX_FRAGMENT_NAME = "reflectFragment.glsl";
	
	constexpr const char* SHADOW_SHADER_VERTEX_FILE_NAME = "shadowSceneVertex.glsl";
	constexpr const char* SHADOW_SHADER_FRAGMENT_FILE_NAME = "shadowSceneFragment.glsl";
}

Renderer::Renderer(Window& parent) : OGLRenderer(parent) {

	initCameraFollowNodes();

	_camera = new Camera(10.0f, 0.0f, Vector3(3859, 375, 4005));
	_camera->initAutoMovement(_planetSceneCameraNodesToVisit, false, nullptr);
	_sceneToggle = true; 


	initMeshes();

	_heightMap = new HeightMap(TEXTUREDIR"islandNoise.jpg");
	initShaders();
	bool isSkyboxShaderInitSuccessfully = _skyboxShader->LoadSuccess();
	bool isLightShaderInitSuccessfully = _lightShader->LoadSuccess();
	bool isSceneShaderIniSuccessfully = _shader->LoadSuccess();
	bool isReflectShaderInitSuccessfully = _reflectShader->LoadSuccess();
	bool isPerPixelSceneShaderInitSuccessfully = _perPixelSceneShader->LoadSuccess();
	bool isAnimationShaderInitSuccessfully = _animationShader->LoadSuccess();
	bool isPostProcessShaderInitSuccessfully = _postProcessShader->LoadSuccess();

	if (!isSceneShaderIniSuccessfully || !isSkyboxShaderInitSuccessfully || !isLightShaderInitSuccessfully
		|| !isReflectShaderInitSuccessfully  || !isPerPixelSceneShaderInitSuccessfully
		|| !isAnimationShaderInitSuccessfully || !isPostProcessShaderInitSuccessfully)
		return;

	initTextures();

	if (!_bumpTexture || !_heightMapTexture || !_waterTex || !_lowPolyTex || !_lowPolyBumpTex || !_treeBumpTex || !_rainTexture)
		return;

	initBuffers();

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	SetTextureRepeating(_heightMapTexture, true);
	SetTextureRepeating(_waterTex, true);
	SetTextureRepeating(_bumpTexture, true);
	InitGlobalSceneNode();
	/*InitSpaceSceneNodes();*/
	InitPlanetSceneNodes();
	_currentSceneRoot = _planetRoot;
	_globalRoot->addChild(_currentSceneRoot);

	projMatrix = Matrix4::Perspective(1.0f, 500000.f, (float)width / (float)height, 90.f);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	init = true;
}

void Renderer::initMeshes() {
	_quad = Mesh::GenerateQuad();
	_sphere = Mesh::LoadFromMeshFile(SPHERE_MESH_FILE_NAME);
	_tree = Mesh::LoadFromMeshFile(TREE_MESH_FILE_NAME);
	_animatedMesh = Mesh::LoadFromMeshFile(ANIMATED_CHARACTER_MESH_FILE_NAME);
	_animateMeshAnimation = new MeshAnimation(ANIMATED_CHARACTER_ANIMATION_FILE_NAME);
	_animatedMeshMaterial = new MeshMaterial(ANIMATED_CHARACTER_MATERIAL_FILE_NAME);
}

void Renderer::initTextures(){
	_treeTexture = SOIL_load_OGL_texture(TEXTUREDIR"TreeDiffuse.png",
		SOIL_LOAD_AUTO,
		SOIL_CREATE_NEW_ID, 0);
	
	_planetCubemap = LoadCubeMap(planetCubeMapPaths);

	_heightMapTexture = SOIL_load_OGL_texture(TEXTUREDIR"Barren Reds.JPG", SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS);
	_bumpTexture = SOIL_load_OGL_texture(TEXTUREDIR"Barren RedsDOT3.JPG", SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS);
	_waterTex = SOIL_load_OGL_texture(TEXTUREDIR"water.TGA", SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS);
	_lowPolyTex = SOIL_load_OGL_texture(TEXTUREDIR"Colors3.png", SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS);
	_lowPolyBumpTex = SOIL_load_OGL_texture(TEXTUREDIR"Colors3DOT3.jpg", SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS);
	_treeBumpTex = SOIL_load_OGL_texture(TEXTUREDIR"TreeDOT3.png", SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS);
	_rainTexture = SOIL_load_OGL_texture(TEXTUREDIR"rain.png", SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, SOIL_FLAG_MIPMAPS);
}

void Renderer::initBuffers(){
	glGenTextures(1, &_shadowTex);
	glBindTexture(GL_TEXTURE_2D, _shadowTex);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);


	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_SIZE, SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);


	glGenFramebuffers(1, &_shadowFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, _shadowFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _shadowTex, 0);
	glDrawBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glGenTextures(1, &_bufferDepthTex);
	glBindTexture(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);

	for (int i = 0; i < 2; ++i) {
		glGenTextures(1, &_bufferColourTex[i]);
		glBindTexture(GL_TEXTURE_2D, _bufferColourTex[i]);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	}

	glGenFramebuffers(1, &_bufferFBO);
	glGenFramebuffers(1, &_postProcessFBO);

	glBindFramebuffer(GL_FRAMEBUFFER, _bufferFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _bufferDepthTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, _bufferDepthTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _bufferColourTex[0], 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE || !_bufferDepthTex || !_bufferColourTex[0]) return;

	glBindFramebuffer(GL_FRAMEBUFFER, _postProcessFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _bufferColourTex[1], 0);
}

void Renderer::initShaders(){
	_shader = new Shader(SCENE_SHADER_VERTEX_FILE_NAME, SCENE_SHADER_FRAGMENT_FILE_NAME);
	_skyboxShader = new Shader(SKYBOX_SHADER_VERTEX_FILE_NAME, SKYBOX_SHADER_FRAGMENT_FILE_NAME);
	_lightShader = new Shader(SCENE_SHADER_VERTEX_WITH_SHADOWS, SCENE_SHADER_FRAGMENT_WITH_SHADOWS);
	_reflectShader = new Shader(REFLECT_SHADER_VERTEX_FILE_NAME, REFLECT_SHADER_VERTEX_FRAGMENT_NAME);
	_shadowShader = new Shader(SHADOW_SHADER_VERTEX_FILE_NAME, SHADOW_SHADER_FRAGMENT_FILE_NAME);
	_perPixelSceneShader = new Shader(SCENE_SHADER_VERTEX_WITH_SHADOWS, SCENE_SHADER_FRAGMENT_WITH_SHADOWS);
	_animationShader = new Shader(ANIMATED_SHADER_VERTEX_FILE_NAME, ANIMATED_SHADER_FRAGMENT_FILE_NAME);
	_postProcessShader = new Shader(POST_PROCESS_SHADER_VERTEX_FILE_NAME, POST_PROCESS_SHADER_FRAGMENT_FILE_NAME);
	_presentShader = new Shader(PRESENT_SHADER_VERTEX_FILE_NAME, PRESENT_SHADER_FRAGMENT_FILE_NAME);
}

void Renderer::initCameraFollowNodes(){
	_spaceSceneCameraNodesToVisit.push_back(Vector3(-812, 341, 2));
	_spaceSceneCameraNodesToVisit.push_back(Vector3(-7417, 1391, -750));
	_spaceSceneCameraNodesToVisit.push_back(Vector3(-16001, 1391, -18753));
	//_spaceSceneCameraNodesToVisit.push_back(Vector3(-33625, 1391, -15717));

	_planetSceneCameraNodesToVisit.push_back(Vector3(3838, 390, 4112));
	_planetSceneCameraNodesToVisit.push_back(Vector3(3771, 390, 5886));
	_planetSceneCameraNodesToVisit.push_back(Vector3(3894, 1730, 7434));
	_planetSceneCameraNodesToVisit.push_back(Vector3(633, 1328, 3171));
	_planetSceneCameraNodesToVisit.push_back(Vector3(1471, 1462, 777));
	_planetSceneCameraNodesToVisit.push_back(Vector3(3456, 1462, 1260));
	_planetSceneCameraNodesToVisit.push_back(Vector3(6052, 1462, 2873));
	_planetSceneCameraNodesToVisit.push_back(Vector3(4851, 1462, 5804));
	_planetSceneCameraNodesToVisit.push_back(Vector3(8510, 7556, -2350));

}

Renderer::~Renderer() {
	/*delete _spaceRoot;*/
	delete _planetRoot;
	delete _quad;
	delete _camera;
	delete _shader;
	glDeleteTextures(1, &_cubeMap);
	glDeleteTextures(1, &_planetCubemap);
}

void Renderer::UpdateScene(float dt) {
	if (Window::GetKeyboard()->KeyTriggered(KEYBOARD_G)) {
		toggleScene();
	}

	if (Window::GetKeyboard()->KeyTriggered(KEYBOARD_V)) {
		_isBlurOn = !_isBlurOn;
	}

	_camera->updateCamera(dt);
	viewMatrix = _camera->buildViewMatrix();
	projMatrix = Matrix4::Perspective(1.f, 500000.f, (float)width / (float)height, 90.f);
	_frameFrustum.fromMatrix(projMatrix * viewMatrix);

	_waterRotate += dt * 2.f; 
	_waterCycle += dt * .25f;

	_globalRoot->update(dt);
}

void Renderer::buildNodeLists(SceneNode* from) {
	bool checkIsInFrustrum = false;
	if (!from->getIsAnimated() || !checkIsInFrustrum || _frameFrustum.isInsideFrustum(*from)) {
		Vector3 dir = from->getWorldTransform().GetPositionVector() - _camera->getPosition();
		from->setCameraDistance(Vector3::Dot(dir, dir));
		if (from->getIsAnimated()) {
			_animatedNodeList.push_back(from);
		}
		else if (from->getColour().w < 1.f) {
			_transparentNodeList.push_back(from);
		}
		else {
			_nodeList.push_back(from);
		}
	}

	for (vector<SceneNode*>::const_iterator i = from->getChildIteratorStart(); i != from->getChildIteratorEnd(); i++) {
		buildNodeLists(*i);
	}
}

void Renderer::sortNodeLists() {
	std::sort(_animatedNodeList.rbegin(), _animatedNodeList.rend(), SceneNode::compareByCameraDistance);
	std::sort(_transparentNodeList.rbegin(), _transparentNodeList.rend(), SceneNode::compareByCameraDistance);
	std::sort(_nodeList.begin(), _nodeList.end(), SceneNode::compareByCameraDistance);
}

void Renderer::drawNodes(bool isDrawingForShadows) {
	for (const auto& i : _animatedNodeList) {
		if (isDrawingForShadows) {
			modelMatrix = i->getWorldTransform() * Matrix4::Scale(i->getModelScale());
			UpdateShaderMatrices();
		}
		else
		{
			i->setUpShader(*this);
		}
		drawNode(i, isDrawingForShadows);
	}
	if (!isDrawingForShadows)
	{
		BindShader(_perPixelSceneShader);
		SetShaderLight(*_currentLight);

		glUniform1i(glGetUniformLocation(_perPixelSceneShader->GetProgram(), "diffuseTex"), 0);

		glUniform1i(glGetUniformLocation(_perPixelSceneShader->GetProgram(), "bumpTex"), 1);

		glUniform1i(glGetUniformLocation(_perPixelSceneShader->GetProgram(), "shadowTex"), 2);

		glUniform3fv(glGetUniformLocation(_perPixelSceneShader->GetProgram(), "cameraPos"), 1, (float*)&_camera->getPosition());
	}

	for (const auto& i : _nodeList) {
		if (isDrawingForShadows) {
			modelMatrix = i->getWorldTransform() * Matrix4::Scale(i->getModelScale());
			UpdateShaderMatrices();
		}
		else
		{
			if (i->getMesh())
			{
				i->setUpShader(*this);
			}
		}
		drawNode(i, isDrawingForShadows);
	}
	for (const auto& i : _transparentNodeList) {
		if (isDrawingForShadows) {
			modelMatrix = i->getWorldTransform() * Matrix4::Scale(i->getModelScale());
			UpdateShaderMatrices();
		}
		else
		{
			i->setUpShader(*this);
		}
		drawNode(i, isDrawingForShadows);
	}
}

void Renderer::drawNode(SceneNode* node, bool isDrawingForShadows) {
	if (node->getMesh()) {
		node->draw(*this, isDrawingForShadows);
	}
}

void Renderer::InitGlobalSceneNode() {

	_globalRoot = new SceneNode();
	_globalRoot->setNodeName("global_node");
}


void Renderer::InitPlanetSceneNodes() {
	_planetRoot = new SceneNode();
	_planetRoot->setNodeName("planet_root");

	Vector3 lightPos = _heightMap->getHeightmapSize() * Vector3(1.1f, 1.1f, 1.1f);
	lightPos.y += 300.f;
	_currentLight = new Light(Vector3(10000.f, 12000.f, -18000.f), Vector4(1, 1, 1, 1), 40000);

	for (int i = 0; i < 200; i++) {
		auto* treeNode = new SceneNode(_tree);
		treeNode->setCamera(_camera);
		treeNode->setLight(_currentLight);

		treeNode->setShader(_perPixelSceneShader);
		treeNode->setColour(Vector4(1.0f, 1.0f, 1.0f, 1.0f));

		/*float modelScale = 10.f * 100;*/
		float modelScale = 10.f * 100 + rand() % 150;  // 150~300 ·¶Î§
		treeNode->setModelScale(Vector3(modelScale, modelScale, modelScale));

		treeNode->setModelScale(Vector3(modelScale, modelScale, modelScale));
		Vector3 mapSize = _heightMap->getHeightmapSize();
		int xRange = rand() % (int)mapSize.x;
		int zRange = rand() % (int)mapSize.z;
		float xPos = (float)(xRange);
		float zPos = (float)(zRange);
		float yPos = _heightMap->getHeightmapSize().y + 200.f;

		Vector3 treePos = Vector3(xPos, yPos, zPos);
		float rot = rand() % 360;
		treeNode->setTransform(
			Matrix4::Translation(Vector3(xPos, yPos, zPos)) *
			Matrix4::Rotation(rot, Vector3(0, 1, 0))
		);
		/*treeNode->setTransform(Matrix4::Translation(treePos));*/
		treeNode->setTexture(_treeTexture);
		treeNode->setShadowTexture(_shadowTex);
		treeNode->setBumpTexture(_treeBumpTex);
		treeNode->setBoundingRadius(40.f);
		treeNode->setNodeName("tree_node_" + i);
		_planetRoot->addChild(treeNode);
		
	}

	auto* animatedNode = new AnimatedSceneNode(_animatedMesh, _animateMeshAnimation, _animatedMeshMaterial);
	animatedNode->setShader(_animationShader);
	animatedNode->setIsAnimated(true);
	animatedNode->setModelScale(Vector3(1.f, 1.f, 1.f));
	animatedNode->setBoundingRadius(1.f);
	animatedNode->setNodeName("animated_node");
	animatedNode->setModelScale(Vector3(100, 100, 100));
	animatedNode->setTransform(Matrix4::Translation(Vector3(3859, 175, 4405)));

	std::vector<Vector3> nodesToFollow;
	nodesToFollow.push_back(Vector3(3633, 175, 6080));
	animatedNode->initMovement(nodesToFollow, false, 50.f);
	_planetRoot->addChild(animatedNode);

	auto* particleSpawnerNode = new ParticleNode(_sphere, _rainTexture);

	particleSpawnerNode->setCamera(_camera);
	particleSpawnerNode->setLight(_currentLight);

	particleSpawnerNode->setShader(_perPixelSceneShader);
	particleSpawnerNode->setColour(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	particleSpawnerNode->setBoundingRadius(1.f);
	_planetRoot->addChild(particleSpawnerNode);

	/*auto* lanternPlant = new SceneNode(_lanternPlant);
	lanternPlant->setBumpTexture(_lowPolyBumpTex);
	lanternPlant->setTexture(_lowPolyTex);
	lanternPlant->setShadowTexture(_shadowTex);
	lanternPlant->setShader(_perPixelSceneShader);
	lanternPlant->setBoundingRadius(5.f);
	lanternPlant->setModelScale(Vector3(100, 100, 100));
	lanternPlant->setTransform(Matrix4::Translation(Vector3(3416, 200, 5612)));
	lanternPlant->setNodeName("lantern_plant_node");
	_planetRoot->addChild(lanternPlant);*/

	m_planetSceneCameraPos = _heightMap->getHeightmapSize() * Vector3(.5f, 1.f, .5f);
	m_planetSceneCameraPos.y += 100.f;
}

void Renderer::InitSkyboxNode() {
	_skyboxNode = new SkyboxNode();
	_skyboxNode->setMesh(_quad);
	_skyboxNode->setShader(_skyboxShader);
	_skyboxNode->setNodeName("Space_SkyboxNode");
	_skyboxNode->setTexture(_cubeMap);

	_skyboxNode->setIsAnimated(false);
}

void Renderer::RenderScene() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	buildNodeLists(_globalRoot);
	sortNodeLists();

	if (_sceneToggle && _isBlurOn)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, _bufferFBO);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}
	else
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}


	drawSkybox();

	if (_sceneToggle) {
		drawShadowScene();
		drawWater();
		drawHeightMap();
		
	}

	drawNodes(false);
	clearNodeLists();


	if (_sceneToggle && _isBlurOn)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, _postProcessFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _bufferColourTex[1], 0);
		modelMatrix.ToIdentity();
		viewMatrix.ToIdentity();
		projMatrix.ToIdentity();
		textureMatrix.ToIdentity();

		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
		drawPostProcess();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		presentScene();
		glEnable(GL_DEPTH_TEST);

		glViewport(0, 0, width, height);
	}

}

void Renderer::clearNodeLists() {
	_transparentNodeList.clear();
	_nodeList.clear();
	_animatedNodeList.clear();
}

void Renderer::toggleScene() {

	_sceneToggle = true;

	
	if (_currentSceneRoot) {
		_globalRoot->removeChild(_currentSceneRoot);
	}

	
	_currentSceneRoot = _planetRoot;
	_globalRoot->addChild(_currentSceneRoot);

	
	if (_camera) {
		_camera->initAutoMovement(
			_planetSceneCameraNodesToVisit,   
			false,                           
			nullptr                           
		);
	}
}

void Renderer::onChangeScene() {
	if (_sceneToggle) {
		_camera->setPosition(_planetSceneCameraNodesToVisit[0]);

		_currentLight->setPosition(Vector3(10000.f, 12000.f, -18000.f));
		_currentLight->setRadius(50000.f);
	}
	else
	{
		_camera->setPosition(_spaceSceneCameraNodesToVisit[0]);
		_currentLight->setPosition(Vector3(20385, 1331, -215));
		_currentLight->setRadius(50000.f);

	}
}

void Renderer::drawShadowScene() {
	glBindFramebuffer(GL_FRAMEBUFFER, _shadowFBO);

	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	viewMatrix = Matrix4::BuildViewMatrix(_currentLight->getPosition() * Vector3(1.0f, 1.0f, 1.0f), _heightMap->getHeightmapSize() * Vector3(0.5f, 0.5f, 0.5f));
	projMatrix = Matrix4::Perspective(1000, 500000.f, 1, 90);

	shadowMatrix = projMatrix * viewMatrix;

	BindShader(_shadowShader);
	drawNodes(true);

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glViewport(0, 0, width, height);
	viewMatrix = _camera->buildViewMatrix();
	projMatrix = Matrix4::Perspective(1.0f, 500000.f, (float)width / (float)height, 90.f);
	glDisable(GL_CULL_FACE);
	if (_isBlurOn)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, _bufferFBO);
	}
	else
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
}

void Renderer::drawPostProcess() {
	BindShader(_postProcessShader);
	UpdateShaderMatrices();

	glDisable(GL_DEPTH_TEST);
	glActiveTexture(GL_TEXTURE0);
	glUniform1i(glGetUniformLocation(_postProcessShader->GetProgram(), "sceneTex"), 0);
	for (int i = 0; i < POST_PASSES; i++) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _bufferColourTex[1], 0);
		glUniform1i(glGetUniformLocation(_postProcessShader->GetProgram(), "isVertical"), 0);
		glBindTexture(GL_TEXTURE_2D, _bufferColourTex[0]);
		_quad->Draw();

		//Swap to colour buffers and do the second blur pass
		glUniform1i(glGetUniformLocation(_postProcessShader->GetProgram(), "isVertical"), 1);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _bufferColourTex[0], 0);
		glBindTexture(GL_TEXTURE_2D, _bufferColourTex[1]);
		_quad->Draw();
	}
}

void Renderer::presentScene() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	BindShader(_presentShader);
	UpdateShaderMatrices();
	glUniform1i(glGetUniformLocation(_presentShader->GetProgram(), "diffuseTex"), 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, _bufferColourTex[1]);

	_quad->Draw();
}

void Renderer::drawHeightMap() {
	BindShader(_perPixelSceneShader);
	SetShaderLight(*_currentLight);

	glUniform1i(glGetUniformLocation(_perPixelSceneShader->GetProgram(), "diffuseTex"), 0);
	glUniform1i(glGetUniformLocation(_perPixelSceneShader->GetProgram(), "bumpTex"), 1);
	glUniform1i(glGetUniformLocation(_perPixelSceneShader->GetProgram(), "shadowTex"), 2);
	glUniform3fv(glGetUniformLocation(_perPixelSceneShader->GetProgram(), "cameraPos"), 1, (float*)&_camera->getPosition());

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, _heightMapTexture);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, _bumpTexture);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, _shadowTex);

	modelMatrix.ToIdentity();
	UpdateShaderMatrices();
	_heightMap->Draw();
}

void Renderer::drawWater() {
	BindShader(_reflectShader);

	glUniform3fv(glGetUniformLocation(_reflectShader->GetProgram(), "cameraPos"), 1, (float*)&_camera->getPosition());
	glUniform1i(glGetUniformLocation(_reflectShader->GetProgram(), "diffuseTex"), 0);
	glUniform1i(glGetUniformLocation(_reflectShader->GetProgram(), "cubeTex"), 1);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, _waterTex);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_CUBE_MAP, _planetCubemap);

	Vector3 hSize = _heightMap->getHeightmapSize();

	modelMatrix =
		Matrix4::Translation(Vector3(hSize.x * 0.5f, hSize.y * 0.80f, hSize.z * 0.5f)) *
		Matrix4::Scale(Vector3(hSize.x * 0.50f, 1.0f, hSize.z * 0.50f)) *  
		Matrix4::Rotation(90, Vector3(1, 0, 0));

	textureMatrix =
		Matrix4::Translation(Vector3(_waterCycle, 0.0f, _waterCycle)) *
		Matrix4::Scale(Vector3(10, 10, 10)) *
		Matrix4::Rotation(_waterRotate, Vector3(0, 0, 1));

	UpdateShaderMatrices();
	_quad->Draw();

	textureMatrix.ToIdentity();
}

void Renderer::drawSkybox() {
	glDepthMask(GL_FALSE);
	BindShader(_skyboxShader);
	glUniform1i(glGetUniformLocation(_skyboxShader->GetProgram(), "cubeTex"), 1);
	glActiveTexture(GL_TEXTURE1);

	if (_sceneToggle)
		glBindTexture(GL_TEXTURE_CUBE_MAP, _planetCubemap);
	else
		glBindTexture(GL_TEXTURE_CUBE_MAP, _cubeMap);

	Matrix4 viewNoTranslation = viewMatrix;
	viewNoTranslation.values[12] = 0;
	viewNoTranslation.values[13] = 0;
	viewNoTranslation.values[14] = 0;

	viewMatrix = viewNoTranslation;
	modelMatrix.ToIdentity();
	textureMatrix.ToIdentity();
	UpdateShaderMatrices();

	_quad->Draw();

	glDepthMask(GL_TRUE);
}
