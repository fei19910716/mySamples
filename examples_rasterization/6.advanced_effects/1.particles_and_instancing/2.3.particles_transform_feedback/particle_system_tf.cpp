
#include "particle_system_tf.h"

#define FOR(q,n) for(int q=0;q<n;q++)
#define SFOR(q,s,e) for(int q=s;q<=e;q++)
#define RFOR(q,n) for(int q=n;q>=0;q--)
#define RSFOR(q,s,e) for(int q=s;q>=e;q--)

#define ESZ(elem) (int)elem.size()

CParticleSystemTransformFeedback::CParticleSystemTransformFeedback()
{
	bInitialized = false;
	iCurReadBuffer = 0;
}

/*-----------------------------------------------

Name:	InitalizeParticleSystem

Params:	none

Result:	Initializes all buffers and data on GPU
		for transform feedback particle system.

/*---------------------------------------------*/

bool CParticleSystemTransformFeedback::InitalizeParticleSystem()
{
	if(bInitialized)return false;

	renderTexture = new Texture2D;
	renderTexture->FromImage(FileSystem::getPath("resources/textures/awesomeface.png").c_str(),false);

	const char* sVaryings[NUM_PARTICLE_ATTRIBUTES] = 
	{
		"vPositionOut",
		"vVelocityOut",
		"vColorOut",
		"fLifeTimeOut",
		"fSizeOut",
		"iTypeOut",
	};

	// Updating program
	spUpdateParticles = new Shader;
	spUpdateParticles->LoadShaderStage("particles_update.vs",GL_VERTEX_SHADER);
	spUpdateParticles->LoadShaderStage("particles_update.gs",GL_GEOMETRY_SHADER);
	spUpdateParticles->LoadShaderStage("particles_update.fs",GL_FRAGMENT_SHADER);
	glTransformFeedbackVaryings(spUpdateParticles->ID, 6, sVaryings, GL_INTERLEAVED_ATTRIBS);
	spUpdateParticles->Link();


	// Rendering program
	spRenderParticles = new Shader;
	spRenderParticles->LoadShaderStage("particles_render.vs",GL_VERTEX_SHADER);
	spRenderParticles->LoadShaderStage("particles_render.gs",GL_GEOMETRY_SHADER);
	spRenderParticles->LoadShaderStage("particles_render.fs",GL_FRAGMENT_SHADER);
	spRenderParticles->Link();

	glGenTransformFeedbacks(1, &uiTransformFeedbackBuffer);
	glGenQueries(1, &uiQuery);

	glGenBuffers(2, uiParticleBuffer);
	glGenVertexArrays(2, uiVAO);

	CParticle partInitialization;
	memset(&partInitialization,0,sizeof(CParticle));
	partInitialization.iType = PARTICLE_TYPE_GENERATOR;
	partInitialization.vPosition = glm::vec3(0.0f, -0.9f, 0.0f);
	partInitialization.fLifeTime = 1.5f;
	partInitialization.fSize = 0.1f;

	FOR(i, 2)
	{	
		glBindVertexArray(uiVAO[i]);
		glBindBuffer(GL_ARRAY_BUFFER, uiParticleBuffer[i]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(CParticle)*MAX_PARTICLES_ON_SCENE, NULL, GL_DYNAMIC_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(CParticle), &partInitialization);

		FOR(i, NUM_PARTICLE_ATTRIBUTES)glEnableVertexAttribArray(i);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(CParticle), (const GLvoid*)0); // Position
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(CParticle), (const GLvoid*)12); // Velocity
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(CParticle), (const GLvoid*)24); // Color
		glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(CParticle), (const GLvoid*)36); // Lifetime
		glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(CParticle), (const GLvoid*)40); // Size
		glVertexAttribPointer(5, 1, GL_INT,	  GL_FALSE, sizeof(CParticle), (const GLvoid*)44); // Type
	}
	iCurReadBuffer = 0;
	iNumParticles = 1;

	bInitialized = true;

	return true;
}

/*-----------------------------------------------

Name:	UpdateParticles

Params:	fTimePassed - time passed since last frame

Result:	Performs particle updating on GPU.

/*---------------------------------------------*/

float grandf(float fMin, float fAdd)
{
	float fRandom = float(rand()%(RAND_MAX+1))/float(RAND_MAX);
	return fMin+fAdd*fRandom;
}

void CParticleSystemTransformFeedback::UpdateParticles(float fTimePassed)
{
	if(!bInitialized)return;

	spUpdateParticles->use();
	spUpdateParticles->SetUniform("fTimePassed",		fTimePassed);
	spUpdateParticles->SetUniform("vGenPosition",		vGenPosition);
	spUpdateParticles->SetUniform("vGenVelocityMin",	vGenVelocityMin);
	spUpdateParticles->SetUniform("vGenVelocityRange",	vGenVelocityRange);
	spUpdateParticles->SetUniform("vGenColor",			vGenColor);
	spUpdateParticles->SetUniform("vGenGravityVector",	vGenGravityVector);
	spUpdateParticles->SetUniform("fGenLifeMin",		fGenLifeMin);
	spUpdateParticles->SetUniform("fGenLifeRange",		fGenLifeRange);
	spUpdateParticles->SetUniform("fGenSize",			fGenSize);
	spUpdateParticles->SetUniform("iNumToGenerate",		0);

	fElapsedTime += fTimePassed;

	if(fElapsedTime > fNextGenerationTime)
	{
		spUpdateParticles->SetUniform("iNumToGenerate", iNumToGenerate);
		fElapsedTime -= fNextGenerationTime;

		glm::vec3 vRandomSeed = glm::vec3(grandf(-10.0f, 20.0f), grandf(-10.0f, 20.0f), grandf(-10.0f, 20.0f));
		spUpdateParticles->SetUniform("vRandomSeed", &vRandomSeed);
	}

	glEnable(GL_RASTERIZER_DISCARD);
	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, uiTransformFeedbackBuffer);

	glBindVertexArray(uiVAO[iCurReadBuffer]);
	glEnableVertexAttribArray(1); // Re-enable velocity
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);
	glEnableVertexAttribArray(5);

	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, uiParticleBuffer[1-iCurReadBuffer]);

	glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, uiQuery);
	glBeginTransformFeedback(GL_POINTS);

	glDrawArrays(GL_POINTS, 0, iNumParticles);

	glEndTransformFeedback();

	glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
	glGetQueryObjectiv(uiQuery, GL_QUERY_RESULT, &iNumParticles);

	// std::vector<CParticle> feedback(3);
	// glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(CParticle) * 3, feedback.data());

	// for(int i = 0; i < 3; i++){
	// 	feedback[i].toString();
	// }
	// std::cout << "----" << std::endl;

	iCurReadBuffer = 1-iCurReadBuffer;

	glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
}

/*-----------------------------------------------

Name:	RenderParticles

Params:	none

Result:	Performs particle rendering on GPU.

/*---------------------------------------------*/

void CParticleSystemTransformFeedback::RenderParticles()
{
	if(!bInitialized)return;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glDepthMask(0);

	glDisable(GL_RASTERIZER_DISCARD);
	spRenderParticles->use();
	spRenderParticles->SetUniform("gVP",matProjection * matView);
	spRenderParticles->SetUniform("gCameraPos",cameraPos);

	renderTexture->Bind(GL_TEXTURE0);

	glBindVertexArray(uiVAO[iCurReadBuffer]);
	glDisableVertexAttribArray(1); // Disable velocity, because we don't need it for rendering
	glDisableVertexAttribArray(2);
	glDisableVertexAttribArray(3);
	glDisableVertexAttribArray(5);

	glDrawArrays(GL_POINTS, 0, iNumParticles);
	// glDrawTransformFeedback(GL_POINTS, tbo);

	glDepthMask(1);	
	glDisable(GL_BLEND);

}

/*-----------------------------------------------

Name:	SetMatrices

Params:	a_matProjection - projection matrix
		vEye, vView, vUpVector - definition of view matrix

Result:	Sets the projection matrix and view matrix,
		that shaders of transform feedback particle system
		need.

/*---------------------------------------------*/

void CParticleSystemTransformFeedback::SetMatrices(glm::mat4* a_matProjection, glm::vec3 vEye, glm::vec3 vView, glm::vec3 vUpVector)
{
	matProjection = *a_matProjection;
	cameraPos = vEye;

	matView = glm::lookAt(vEye, vView, vUpVector);
	vView = vView-vEye;
	vView = glm::normalize(vView);
	vQuad1 = glm::cross(vView, vUpVector);
	vQuad1 = glm::normalize(vQuad1);
	vQuad2 = glm::cross(vView, vQuad1);
	vQuad2 = glm::normalize(vQuad2);
}

/*-----------------------------------------------

Name:	SetGeneratorProperties

Params:	many properties of particle generation

Result:	Guess what it does :)

/*---------------------------------------------*/

void CParticleSystemTransformFeedback::SetGeneratorProperties(
	glm::vec3 a_vGenPosition, 
	glm::vec3 a_vGenVelocityMin, 
	glm::vec3 a_vGenVelocityMax, 
	glm::vec3 a_vGenGravityVector, 
	glm::vec3 a_vGenColor, 
	float a_fGenLifeMin, 
	float a_fGenLifeMax, 
	float a_fGenSize, 
	float fEvery, 
	int a_iNumToGenerate)
{
	vGenPosition = a_vGenPosition;
	vGenVelocityMin = a_vGenVelocityMin;
	vGenVelocityRange = a_vGenVelocityMax - a_vGenVelocityMin;

	vGenGravityVector = a_vGenGravityVector;
	vGenColor = glm::vec3(0.0f, 0.5f, 1.0f);
	fGenSize = a_fGenSize;

	fGenLifeMin = a_fGenLifeMin;
	fGenLifeRange = a_fGenLifeMax - a_fGenLifeMin;

	fNextGenerationTime = fEvery;
	fElapsedTime = 0.0f;

	iNumToGenerate = a_iNumToGenerate;
}

/*-----------------------------------------------

Name:	GetNumParticles

Params:	none

Result:	Retrieves current number of particles.

/*---------------------------------------------*/

int CParticleSystemTransformFeedback::GetNumParticles()
{
	return iNumParticles;
}