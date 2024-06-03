#define STB_IMAGE_IMPLEMENTATION

#include <stdio.h>
#include <string.h>
#include <cmath>
#include <vector>

#include <GL\glew.h>
#include <GLFW\glfw3.h>

#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <glm\gtc\type_ptr.hpp>

#include "CommonValues.h"

#include "Window.h"
#include "Mesh.h"
#include "Shader.h"
#include "Camera.h"
#include "Texture.h"
#include "DirectionalLight.h"
#include "PointLight.h"
#include "SpotLight.h"
#include "Material.h"

#include "Model.h"

#include "Skybox.h"

const float NaRadiany = 3.14159265f / 180.0f;

GLuint uniformProjection = 0, uniformModel = 0, uniformView = 0, uniformEyePosition = 0,
uniformSpecularIntensity = 0, uniformShininess = 0,
uniformDirectionalLightTransform = 0, uniformOmniLightPos = 0, uniformFarPlane = 0;

Window mainWindow;
std::vector<Mesh*> meshList;

std::vector<Shader> shaderList;
Shader directionalShadowShader;
Shader omniShadowShader;

Camera camera;

Texture brickTexture;
Texture dirtTexture;
Texture plainTexture;

Material shinyMaterial;
Material dullMaterial;

Model Helikopter;
Model Drzewo;
Model Domek;
Model Kwiat;

DirectionalLight mainLight;
PointLight pointLights[MAX_POINT_LIGHTS];
SpotLight spotLights[MAX_SPOT_LIGHTS];

Skybox skybox;

unsigned int pointLightCount = 0;
unsigned int spotLightCount = 0;

GLfloat deltaTime = 0.0f;
GLfloat lastTime = 0.0f;

GLfloat HelikopterAngle = 0.0f;

// Vertex Shader
static const char* vShader = "Shaders/shader.vert";

// Fragment Shader
static const char* fShader = "Shaders/shader.frag";

void calcAverageNormals(unsigned int* indices, unsigned int indiceCount, GLfloat* vertices, unsigned int verticeCount,
	unsigned int vLength, unsigned int normalOffset)
{
	for (size_t i = 0; i < indiceCount; i += 3)
	{
		unsigned int in0 = indices[i] * vLength;
		unsigned int in1 = indices[i + 1] * vLength;
		unsigned int in2 = indices[i + 2] * vLength;
		glm::vec3 v1(vertices[in1] - vertices[in0], vertices[in1 + 1] - vertices[in0 + 1], vertices[in1 + 2] - vertices[in0 + 2]);
		glm::vec3 v2(vertices[in2] - vertices[in0], vertices[in2 + 1] - vertices[in0 + 1], vertices[in2 + 2] - vertices[in0 + 2]);
		glm::vec3 normal = glm::cross(v1, v2);
		normal = glm::normalize(normal);

		in0 += normalOffset; in1 += normalOffset; in2 += normalOffset;
		vertices[in0] += normal.x; vertices[in0 + 1] += normal.y; vertices[in0 + 2] += normal.z;
		vertices[in1] += normal.x; vertices[in1 + 1] += normal.y; vertices[in1 + 2] += normal.z;
		vertices[in2] += normal.x; vertices[in2 + 1] += normal.y; vertices[in2 + 2] += normal.z;
	}

	for (size_t i = 0; i < verticeCount / vLength; i++)
	{
		unsigned int nOffset = i * vLength + normalOffset;
		glm::vec3 vec(vertices[nOffset], vertices[nOffset + 1], vertices[nOffset + 2]);
		vec = glm::normalize(vec);
		vertices[nOffset] = vec.x; vertices[nOffset + 1] = vec.y; vertices[nOffset + 2] = vec.z;
	}
}

void CreateObjects()
{
	unsigned int indices[] =
	{
		0, 3, 1,
		1, 3, 2,
		2, 3, 0,
		0, 1, 2
	};

	GLfloat vertices[] =
	{
		//	 x      y      z		u	  v			nx	  ny    nz
			-1.0f, -1.0f, -0.6f,	0.0f, 0.0f,		0.0f, 0.0f, 0.0f,
			0.0f, -1.0f, 1.0f,		0.5f, 0.0f,		0.0f, 0.0f, 0.0f,
			1.0f, -1.0f, -0.6f,		1.0f, 0.0f,		0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f,		0.5f, 1.0f,		0.0f, 0.0f, 0.0f
	};

	// Indeksy dla pod�ogi

	unsigned int floorIndices[] = 
	{
		0, 2, 1,
		1, 2, 3
	};
	

	// xyz,uv, nx,ny,nz dla pod�ogi
	GLfloat floorVertices[] = 
	{
		-10.0f, 0.0f, -10.0f,	0.0f, 0.0f,		0.0f, -1.0f, 0.0f,
		10.0f, 0.0f, -10.0f,	10.0f, 0.0f,	0.0f, -1.0f, 0.0f,
		-10.0f, 0.0f, 10.0f,	0.0f, 10.0f,	0.0f, -1.0f, 0.0f,
		10.0f, 0.0f, 10.0f,		10.0f, 10.0f,	0.0f, -1.0f, 0.0f
	};

	calcAverageNormals(indices, 12, vertices, 32, 8, 5);

	Mesh* obj = new Mesh();
	obj->CreateMesh(floorVertices, floorIndices, 32, 6);
	meshList.push_back(obj);
}

void CreateShaders()
{
	Shader* shader1 = new Shader();
	shader1->CreateFromFiles(vShader, fShader);
	shaderList.push_back(*shader1);

	directionalShadowShader.CreateFromFiles("Shaders/directional_shadow_map.vert", "Shaders/directional_shadow_map.frag");
	omniShadowShader.CreateFromFiles("Shaders/omni_shadow_map.vert", "Shaders/omni_shadow_map.geom", "Shaders/omni_shadow_map.frag");
}

void RenderScene()
{

	// Poszczeg�lne ustawienia dla ka�dego z obiekt�w:

    // Najpierw tworzymy model,
	// nast�pnie umieszczamy go (translate)
	// mo�na go obr�ci� (rotate)
	// i powi�kszy� zmniejszy� (scale)
	// I tak dla ka�dego po kolei 
	// Na koniec dodajemy tekstury
	// Nak�adamy materia�
	// i renderujemy

	glm::mat4 model(1.0f);

	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(0.0f, -2.0f, 0.0f));
	model = glm::scale(model, glm::vec3(11.5f, 0.5f, 11.5f));
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
	dirtTexture.UseTexture();
	shinyMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	meshList[0]->RenderMesh();

	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(-2.0f, -1.5f, 10.0f));
	model = glm::scale(model, glm::vec3(0.5f, 0.5f, 0.5f));
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
	dullMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	Drzewo.RenderModel();

	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(-2.0f, -1.5f, 10.0f));
	model = glm::scale(model, glm::vec3(0.5f, 0.5f, 0.5f));
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
	dullMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	Drzewo.RenderModel();

	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(-9.0f, -1.5f, 10.0f));
	model = glm::scale(model, glm::vec3(0.5f, 0.5f, 0.5f));
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
	dullMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	Drzewo.RenderModel();

	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(-19.0f, -1.5f, 20.0f));
	model = glm::scale(model, glm::vec3(0.5f, 0.5f, 0.5f));
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
	dullMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	Drzewo.RenderModel();

	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(12.f, -1.5f, 1.0f));
	model = glm::scale(model, glm::vec3(0.5f, 0.5f, 0.5f));
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
	dullMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	Drzewo.RenderModel();

	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(8.0f, -2.0f, 10.0f));
	model = glm::scale(model, glm::vec3(1.f, 1.f, 1.f));
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
	dullMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	Domek.RenderModel();


	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(-20.0f, -6.0f, 10.0f));
	model = glm::rotate(model, -90.0f * NaRadiany, glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::scale(model, glm::vec3(2.f, 2.f, 2.f));
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
	dullMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	Kwiat.RenderModel();

	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(-20.0f, -6.0f, 10.0f));
	model = glm::rotate(model, -90.0f * NaRadiany, glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::scale(model, glm::vec3(2.f, 2.f, 2.f));
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
	dullMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	Kwiat.RenderModel();

	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(20.0f, -6.0f, -10.0f));
	model = glm::rotate(model, -90.0f * NaRadiany, glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::scale(model, glm::vec3(2.f, 2.f, 2.f));
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
	dullMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	Kwiat.RenderModel();

	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(-20.0f, -6.0f, -10.0f));
	model = glm::rotate(model, -90.0f * NaRadiany, glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::scale(model, glm::vec3(2.f, 2.f, 2.f));
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
	dullMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	Kwiat.RenderModel();

	//  W k�ko zmieniamy obr�t helikoptera
	HelikopterAngle += 0.1f;
	if (HelikopterAngle > 360.0f) // Reset
	{
		HelikopterAngle = 0.1f;
	}

	model = glm::mat4(1.0f);
	//model = glm::rotate(model, -HelikopterAngle * NaRadiany, glm::vec3(0.0f, 1.0f, 0.0f));
	model = glm::translate(model, glm::vec3(-8.0f, 24.0f, 0.0f));
	model = glm::rotate(model, -20.0f * NaRadiany, glm::vec3(0.0f, 0.0f, 1.0f));
	model = glm::rotate(model, -90.0f * NaRadiany, glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::scale(model, glm::vec3(0.6f, 0.6f, 0.6f));
	glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
	shinyMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
	Helikopter.RenderModel();
}

void DirectionalShadowMapPass(DirectionalLight* light)
{
	directionalShadowShader.UseShader();

	glViewport(0, 0, light->getShadowMap()->GetShadowWidth(), light->getShadowMap()->GetShadowHeight());

	light->getShadowMap()->Write();
	glClear(GL_DEPTH_BUFFER_BIT);

	uniformModel = directionalShadowShader.GetModelLocation();
	directionalShadowShader.Validate();

	RenderScene();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OmniShadowMapPass(PointLight* light)
{
	omniShadowShader.UseShader();

	glViewport(0, 0, light->getShadowMap()->GetShadowWidth(), light->getShadowMap()->GetShadowHeight());

	light->getShadowMap()->Write();
	glClear(GL_DEPTH_BUFFER_BIT);

	uniformModel = omniShadowShader.GetModelLocation();
	uniformOmniLightPos = omniShadowShader.GetOmniLightPosLocation();
	uniformFarPlane = omniShadowShader.GetFarPlaneLocation();

	glUniform3f(uniformOmniLightPos, light->GetPosition().x, light->GetPosition().y, light->GetPosition().z);
	glUniform1f(uniformFarPlane, light->GetFarPlane());
	omniShadowShader.SetLightMatrices(light->CalculateLightTransform());

	omniShadowShader.Validate();

	RenderScene();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderPass(glm::mat4 viewMatrix, glm::mat4 projectionMatrix)
{
	glViewport(0, 0, 1920, 1024);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	skybox.DrawSkybox(viewMatrix, projectionMatrix);

	shaderList[0].UseShader();

	uniformModel = shaderList[0].GetModelLocation();
	uniformProjection = shaderList[0].GetProjectionLocation();
	uniformView = shaderList[0].GetViewLocation();
	uniformModel = shaderList[0].GetModelLocation();
	uniformEyePosition = shaderList[0].GetEyePositionLocation();
	uniformSpecularIntensity = shaderList[0].GetSpecularIntensityLocation();
	uniformShininess = shaderList[0].GetShininessLocation();

	glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projectionMatrix));
	glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(viewMatrix));
	glUniform3f(uniformEyePosition, camera.getCameraPosition().x, camera.getCameraPosition().y, camera.getCameraPosition().z);

	shaderList[0].SetDirectionalLight(&mainLight);
	shaderList[0].SetPointLights(pointLights, pointLightCount, 3, 0);
	shaderList[0].SetSpotLights(spotLights, spotLightCount, 3 + pointLightCount, pointLightCount);

	mainLight.getShadowMap()->Read(GL_TEXTURE2);
	shaderList[0].SetTexture(1);
	shaderList[0].SetDirectionalShadowMap(2);

	glm::vec3 lowerLight = camera.getCameraPosition();
	lowerLight.y -= 0.3f;
	spotLights[0].SetFlash(lowerLight, camera.getCameraDirection());

	shaderList[0].Validate();

	RenderScene();
}

int main()
{
	mainWindow = Window(1920, 1024); // Rozdzielczo�� ekranu
	mainWindow.Initialise();

	CreateObjects();
	CreateShaders();

	camera = Camera(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), -60.0f, 0.0f, 5.0f, 0.5f);

	// Wgrywanie tekstur
	brickTexture = Texture("Textures/brick.png");
	brickTexture.LoadTextureA();
	dirtTexture = Texture("Textures/dirt.png");
	dirtTexture.LoadTextureA();
	plainTexture = Texture("Textures/plain.png");
	plainTexture.LoadTextureA();

	// Tworzenie materia��w  ( intensywno�� �wiat�a, jasno�� , od 1 do 256 zazwyczaj)
	shinyMaterial = Material(4.0f, 256);
	dullMaterial = Material(0.3f, 4);

	// Modele

	Drzewo = Model();
	Drzewo.LoadModel("Models/Lowpoly_tree_sample.obj");
	Domek = Model();
	Domek.LoadModel("Models/Cottage_FREE.obj");
	Helikopter = Model();
	Helikopter.LoadModel("Models/uh60.obj");
	Kwiat = Model();
	Kwiat.LoadModel("Models/12978_tulip_flower_l3.obj");



	mainLight = DirectionalLight
		(2048, 2048,
		1.0f, 0.53f, 0.3f,
		0.1f, 0.9f,
		-10.0f, -12.0f, 18.5f);

	pointLights[0] = PointLight
		(1024, 1024,
		0.01f, 100.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 1.0f,
		1.0f, 2.0f, 0.0f,
		0.3f, 0.2f, 0.1f);
	pointLightCount++;
	pointLights[1] = PointLight
		(1024, 1024,
		0.01f, 100.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f,
		-4.0f, 3.0f, 0.0f,
		0.3f, 0.2f, 0.1f);
	pointLightCount++;


	spotLights[0] = SpotLight
		(1024, 1024,
		0.01f, 100.0f,
		1.0f, 1.0f, 1.0f,
		0.0f, 2.0f,
		0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		20.0f);
	spotLightCount++;
	spotLights[1] = SpotLight
		(1024, 1024,
		0.01f, 100.0f,
		1.0f, 1.0f, 1.0f,
		0.0f, 1.0f,
		0.0f, -1.5f, 0.0f,
		-100.0f, -1.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		20.0f);
	spotLightCount++;

	std::vector<std::string> skyboxFaces;
	skyboxFaces.push_back("Textures/Skybox/cupertin-lake_rt.tga");
	skyboxFaces.push_back("Textures/Skybox/cupertin-lake_lf.tga");
	skyboxFaces.push_back("Textures/Skybox/cupertin-lake_up.tga");
	skyboxFaces.push_back("Textures/Skybox/cupertin-lake_dn.tga");
	skyboxFaces.push_back("Textures/Skybox/cupertin-lake_bk.tga");
	skyboxFaces.push_back("Textures/Skybox/cupertin-lake_ft.tga");

	skybox = Skybox(skyboxFaces);

	GLuint uniformProjection = 0, uniformModel = 0, uniformView = 0, uniformEyePosition = 0,
			uniformSpecularIntensity = 0, uniformShininess = 0;
	glm::mat4 projection = glm::perspective(glm::radians(60.0f), (GLfloat)mainWindow.getBufferWidth() / mainWindow.getBufferHeight(), 0.1f, 100.0f);

	// Do konca trwania okienka 
	while (!mainWindow.getShouldClose())
	{
		GLfloat now = glfwGetTime(); // Liczymy czas
		deltaTime = now - lastTime; // Delta time to roznica miedzy teraz a tym co bylo
		lastTime = now;

		// Pobieranie Klikni�� i event�w
		glfwPollEvents();

		// Pobeiranie klawiszy i sterowania myszk�
		camera.keyControl(mainWindow.getsKeys(), deltaTime);
		camera.mouseControl(mainWindow.getXChange(), mainWindow.getYChange());

		// Mo�na w��czy�/wy��czy� latark� za pomoc� L
		if (mainWindow.getsKeys()[GLFW_KEY_L])
		{
			spotLights[0].Toggle();
			mainWindow.getsKeys()[GLFW_KEY_L] = false;
		}

		DirectionalShadowMapPass(&mainLight);
		for (size_t i = 0; i < pointLightCount; i++)
		{
			OmniShadowMapPass(&pointLights[i]);
		}
		for (size_t i = 0; i < spotLightCount; i++)
		{
			OmniShadowMapPass(&spotLights[i]);
		}
		RenderPass(camera.calculateViewMatrix(), projection);

		mainWindow.swapBuffers();
	}

	return 0;
}