#include "Game.h"

#include <iostream>
#include <fstream>
#include <string>
#include <cmath>

Game::Game(const std::string& config)
{
	init(config);
}

void Game::init(const std::string& path)
{
	std::string temp, fontFile;
	int windowWidth = 0, windowHeight = 0, frameLimit = 0, fontR = 0, fontG = 0, fontB = 0;
	bool fullScreen;

	std::fstream fin(path);

	// Reading window info
	fin >> temp;
	fin >> windowWidth >> windowHeight >> frameLimit >> fullScreen;

	// Reading font info
	fin >> temp;
	fin >> fontFile >> m_fontSize >> fontR >> fontG >> fontB;

	// Reading player info
	fin >> temp;
	fin >> m_playerConfig.SR >> m_playerConfig.CR >> m_playerConfig.S >> m_playerConfig.FR >> m_playerConfig.FG >> m_playerConfig.FB
		>> m_playerConfig.OR >> m_playerConfig.OG >> m_playerConfig.OB >> m_playerConfig.OT >> m_playerConfig.V;

	// Reading enemy info
	fin >> temp;
	fin >> m_enemyConfig.SR >> m_enemyConfig.CR >> m_enemyConfig.SMIN >> m_enemyConfig.SMAX >> m_enemyConfig.OR >> m_enemyConfig.OG
		>> m_enemyConfig.OB >> m_enemyConfig.OT >> m_enemyConfig.VMIN >> m_enemyConfig.VMAX >> m_enemyConfig.L >> m_enemyConfig.SI;

	// Reading bullet info
	fin >> temp;
	fin >> m_bulletConfig.SR >> m_bulletConfig.CR >> m_bulletConfig.S >> m_bulletConfig.FR >> m_bulletConfig.FG >> m_bulletConfig.FB
		>> m_bulletConfig.OR >> m_bulletConfig.OG >> m_bulletConfig.OB >> m_bulletConfig.OT >> m_bulletConfig.V >> m_bulletConfig.L;

	m_window.create(sf::VideoMode(windowWidth, windowHeight), "Geomatry Wars", (fullScreen ? sf::Style::Fullscreen : sf::Style::Resize|sf::Style::Close));
	m_window.setFramerateLimit(frameLimit);

	spawnPlayer();

	// Initialize score text

	if (!m_font.loadFromFile(fontFile))
	{
		std::cout << "Error loading font!" << std::endl;
	}
	m_text.setFont(m_font);
	m_text.setCharacterSize(m_fontSize);
	m_fontColor = sf::Color(fontR, fontG, fontB);
	m_text.setFillColor(m_fontColor);
	m_text.setPosition(5, 5);
}

void Game::run()
{
	while (m_running)
	{
		m_entities.update();

		if (!m_paused)
		{
			sEnemySpawner();
			sMovement();
			sCollision();
			sLifespan();
		}

		sUserInput();
		sRender();

		m_currentFrame++;
	}
}

void Game::setPaused(bool paused)
{
	m_paused = paused;
}

void Game::spawnPlayer()
{
	auto entity = m_entities.addEntity("player");

	float mx = m_window.getSize().x / 2.0f;
	float my = m_window.getSize().y / 2.0f;

	entity->cTransform = std::make_shared<CTransform>(Vec2(mx, my), Vec2(m_playerConfig.S, m_playerConfig.S), 0.0f);

	entity->cShape = std::make_shared<CShape>(
		m_playerConfig.SR,
		m_playerConfig.V,
		sf::Color(m_playerConfig.FR, m_playerConfig.FG, m_playerConfig.FB),
		sf::Color(m_playerConfig.OR, m_playerConfig.OG, m_playerConfig.OB),
		m_playerConfig.OT
	);

	entity->cInput = std::make_shared<CInput>();

	entity->cCollision = std::make_shared<CCollision>(m_playerConfig.CR);

	m_player = entity;
}

void Game::spawnEnemy()
{
	auto entity = m_entities.addEntity("enemy");

	float ex = rand() % ((m_window.getSize().x - m_enemyConfig.SR) - m_enemyConfig.SR + 1) + m_enemyConfig.SR;
	float ey = rand() % ((m_window.getSize().y - m_enemyConfig.SR) - m_enemyConfig.SR + 1) + m_enemyConfig.SR;

	float speed = rand() % (int)(m_enemyConfig.SMAX - m_enemyConfig.SMIN + 1) + m_enemyConfig.SMIN;
	float vertices = rand() % (int)(m_enemyConfig.VMAX - m_enemyConfig.VMIN + 1) + m_enemyConfig.VMIN;
	int colorR = rand() % 256;
	int colorG = rand() % 256;
	int colorB = rand() % 256;

	entity->cTransform = std::make_shared<CTransform>(Vec2(ex, ey), Vec2(speed, speed), 0.0f);

	entity->cShape = std::make_shared<CShape>(
		m_enemyConfig.SR,
		vertices,
		sf::Color(colorR, colorG, colorB),
		sf::Color(m_enemyConfig.OR, m_enemyConfig.OG, m_enemyConfig.OB),
		m_enemyConfig.OT
	);

	entity->cCollision = std::make_shared<CCollision>(m_enemyConfig.CR);

	entity->cScore = std::make_shared<CScore>(vertices * 100);

	m_lastEnemySpawnTime = m_currentFrame;
}

void Game::spawnSmallEnemies(std::shared_ptr<Entity> e)
{
	int vertices = e->cShape->circle.getPointCount();
	Vec2 oldVelocity = e->cTransform->velocity;

	for (int i = 0; i < vertices; i++)
	{
		auto entity = m_entities.addEntity("small_enemy");

		Vec2 newVelocity(oldVelocity.x * cos(6.28319 / vertices * i), oldVelocity.y * sin(6.28319 / vertices * i));

		entity->cTransform = std::make_shared<CTransform>(e->cTransform->pos, newVelocity, 0.0f);

		entity->cShape = std::make_shared<CShape>(
			e->cShape->circle.getRadius() / 2.0f,
			vertices,
			e->cShape->circle.getFillColor(),
			e->cShape->circle.getOutlineColor(),
			e->cShape->circle.getOutlineThickness() / 2.0f
		);

		entity->cLifespan = std::make_shared<CLifespan>(m_enemyConfig.L);

		entity->cCollision = std::make_shared<CCollision>(m_enemyConfig.CR / 2.0f);

		entity->cScore = std::make_shared<CScore>(vertices * 200);
	}
}

void Game::spawnBullets(std::shared_ptr<Entity> entity, const Vec2& target)
{
	auto bullet = m_entities.addEntity("bullet");

	Vec2 diff = target - entity->cTransform->pos;
	diff.normalize();

	Vec2 velocity = diff * m_bulletConfig.S;

	bullet->cTransform = std::make_shared<CTransform>(entity->cTransform->pos, Vec2(velocity.x, velocity.y), 0.0f);

	bullet->cShape = std::make_shared<CShape>(
		m_bulletConfig.SR,
		m_bulletConfig.V,
		sf::Color(m_bulletConfig.FR, m_bulletConfig.FG, m_bulletConfig.FB),
		sf::Color(m_bulletConfig.OR, m_bulletConfig.OG, m_bulletConfig.OB),
		m_bulletConfig.OT
	);

	bullet->cLifespan = std::make_shared<CLifespan>(m_bulletConfig.L);

	bullet->cCollision = std::make_shared<CCollision>(m_bulletConfig.CR);
}

void Game::spawnSpecialWeapon(std::shared_ptr<Entity> entity)
{
	if (m_currentFrame - m_lastSpecialWeaponUsedTime > m_specialWeaponCoolDownTime)
	{
		int bandCount = 20;
		for (int i = 0; i < bandCount; i++)
		{
			auto weapon = m_entities.addEntity("special_weapon");

			weapon->cTransform = std::make_shared<CTransform>(entity->cTransform->pos, Vec2(0.0f, 0.0f), 0.0f);

			weapon->cShape = std::make_shared<CShape>(
				entity->cShape->circle.getRadius(),
				32,
				sf::Color::Transparent,
				sf::Color::Red,
				2
			);

			weapon->cLifespan = std::make_shared<CLifespan>(40);

			weapon->cTiming = std::make_shared<CTiming>(10 * i);

			// Collision radius = player radius + lifespan * radius scaling rate
			weapon->cCollision = std::make_shared<CCollision>(m_playerConfig.SR + 40 * 2);
		}
	}
}

void Game::sMovement()
{
	// Player movements

	m_player->cTransform->velocity = { 0, 0 };

	if (m_player->cInput->up)
	{
		if (m_player->cTransform->pos.y - m_playerConfig.SR >= 0)
		{
			m_player->cTransform->velocity.y = m_playerConfig.S * -1;
		}
	}
	if (m_player->cInput->down)
	{
		if (m_player->cTransform->pos.y + m_playerConfig.SR <= m_window.getSize().y)
		{
			m_player->cTransform->velocity.y = m_playerConfig.S;
		}
	}
	if (m_player->cInput->left)
	{
		if (m_player->cTransform->pos.x - m_playerConfig.SR >= 0)
		{
			m_player->cTransform->velocity.x = m_playerConfig.S * -1;
		}
	}
	if (m_player->cInput->right)
	{
		if (m_player->cTransform->pos.x + m_playerConfig.SR <= m_window.getSize().x)
		{
			m_player->cTransform->velocity.x = m_playerConfig.S;
		}
	}

	m_player->cTransform->pos += m_player->cTransform->velocity;

	// Enemy movements

	for (auto& e : m_entities.getEntities("enemy"))
	{
		e->cTransform->pos += e->cTransform->velocity;

		if (e->cTransform->pos.y - m_enemyConfig.SR < 0 || e->cTransform->pos.y + m_enemyConfig.SR > m_window.getSize().y)
		{
			e->cTransform->velocity.y *= -1;
		}
		if (e->cTransform->pos.x - m_enemyConfig.SR < 0 || e->cTransform->pos.x + m_enemyConfig.SR > m_window.getSize().x)
		{
			e->cTransform->velocity.x *= -1;
		}
	}

	// Bullet movements

	for (auto& e : m_entities.getEntities("bullet"))
	{
		e->cTransform->pos += e->cTransform->velocity;
	}

	// Small enemy movements

	for (auto& e : m_entities.getEntities("small_enemy"))
	{
		e->cTransform->pos += e->cTransform->velocity;
	}

	// Special weapon movements

	for (auto& e : m_entities.getEntities("special_weapon"))
	{
		e->cTransform->pos = m_player->cTransform->pos;

		if (e->cTiming->startTime <= 0)
		{
			float newRadius = e->cShape->circle.getRadius() + 2;
			e->cShape->circle.setRadius(newRadius);
			e->cShape->circle.setOrigin(newRadius, newRadius);
		}
		else {
			e->cTiming->startTime--;
		}
	}
}

void Game::sLifespan()
{
	// Bullet lifespan

	for (auto& e : m_entities.getEntities("bullet"))
	{
		if (e->cLifespan->remaining <= 0)
		{
			e->destroy();
		}
		else
		{
			sf::Color fillColor = e->cShape->circle.getFillColor();
			e->cShape->circle.setFillColor(sf::Color(fillColor.r, fillColor.g, fillColor.b, 255 / e->cLifespan->total * e->cLifespan->remaining));

			sf::Color outlineColor = e->cShape->circle.getOutlineColor();
			e->cShape->circle.setOutlineColor(sf::Color(outlineColor.r, outlineColor.g, outlineColor.b, 255 / e->cLifespan->total * e->cLifespan->remaining));

			e->cLifespan->remaining--;
		}
	}

	// Small enemy lifespan

	for (auto& e : m_entities.getEntities("small_enemy"))
	{
		if (e->cLifespan->remaining <= 0)
		{
			e->destroy();
		}
		else
		{
			sf::Color fillColor = e->cShape->circle.getFillColor();
			e->cShape->circle.setFillColor(sf::Color(fillColor.r, fillColor.g, fillColor.b, 255 / e->cLifespan->total * e->cLifespan->remaining));

			sf::Color outlineColor = e->cShape->circle.getOutlineColor();
			e->cShape->circle.setOutlineColor(sf::Color(outlineColor.r, outlineColor.g, outlineColor.b, 255 / e->cLifespan->total * e->cLifespan->remaining));

			e->cLifespan->remaining--;
		}
	}

	// Special weapon lifespan

	for (auto& e : m_entities.getEntities("special_weapon"))
	{
		if (e->cTiming->startTime <= 0)
		{
			if (e->cLifespan->remaining <= 0)
			{
				m_lastSpecialWeaponUsedTime = m_currentFrame;
				e->destroy();
			}
			else
			{
				sf::Color outlineColor = e->cShape->circle.getOutlineColor();
				e->cShape->circle.setOutlineColor(sf::Color(outlineColor.r, outlineColor.g, outlineColor.b, 255 / e->cLifespan->total * e->cLifespan->remaining));

				e->cLifespan->remaining--;
			}
		}
	}
}

void Game::sCollision()
{
	// Bullet and enemy collision

	for (auto b : m_entities.getEntities("bullet"))
	{
		for (auto e : m_entities.getEntities("enemy"))
		{
			Vec2 diff = b->cTransform->pos - e->cTransform->pos;

			if ((diff.x * diff.x + diff.y * diff.y) < ((b->cCollision->radius + e->cCollision->radius) * (b->cCollision->radius + e->cCollision->radius)))
			{
				m_score += e->cScore->score;
				spawnSmallEnemies(e);
				e->destroy();
				b->destroy();
			}
		}
	}

	// Bullet and small enemy collision

	for (auto b : m_entities.getEntities("bullet"))
	{
		for (auto e : m_entities.getEntities("small_enemy"))
		{
			Vec2 diff = b->cTransform->pos - e->cTransform->pos;

			if ((diff.x * diff.x + diff.y * diff.y) < ((b->cCollision->radius + e->cCollision->radius) * (b->cCollision->radius + e->cCollision->radius)))
			{
				m_score += e->cScore->score;
				e->destroy();
				b->destroy();
			}
		}
	}

	// Player and enemy collision

	for (auto e : m_entities.getEntities("enemy"))
	{
		for (auto p : m_entities.getEntities("player"))
		{
			Vec2 diff = e->cTransform->pos - p->cTransform->pos;

			if ((diff.x * diff.x + diff.y * diff.y) < ((e->cCollision->radius + p->cCollision->radius) * (e->cCollision->radius + p->cCollision->radius)))
			{
				float mx = m_window.getSize().x / 2.0f;
				float my = m_window.getSize().y / 2.0f;

				p->cTransform->pos = Vec2(mx, my);
			}
		}
	}

	// Player and small enemy collision

	for (auto e : m_entities.getEntities("small_enemy"))
	{
		for (auto p : m_entities.getEntities("player"))
		{
			Vec2 diff = e->cTransform->pos - p->cTransform->pos;

			if ((diff.x * diff.x + diff.y * diff.y) < ((e->cCollision->radius + p->cCollision->radius) * (e->cCollision->radius + p->cCollision->radius)))
			{
				float mx = m_window.getSize().x / 2.0f;
				float my = m_window.getSize().y / 2.0f;

				p->cTransform->pos = Vec2(mx, my);
			}
		}
	}

	// Special weapon and enemy collision

	for (auto e : m_entities.getEntities("enemy"))
	{
		for (auto w : m_entities.getEntities("special_weapon"))
		{
			Vec2 diff = e->cTransform->pos - w->cTransform->pos;

			if ((diff.x * diff.x + diff.y * diff.y) < ((e->cCollision->radius + w->cCollision->radius) * (e->cCollision->radius + w->cCollision->radius)))
			{
				m_score += e->cScore->score;
				spawnSmallEnemies(e);
				e->destroy();
			}
		}
	}

	// Special weapon and small enemy collision

	for (auto e : m_entities.getEntities("small_enemy"))
	{
		for (auto w : m_entities.getEntities("special_weapon"))
		{
			Vec2 diff = e->cTransform->pos - w->cTransform->pos;

			if ((diff.x * diff.x + diff.y * diff.y) < ((e->cCollision->radius + w->cCollision->radius) * (e->cCollision->radius + w->cCollision->radius)))
			{
				m_score += e->cScore->score;
				e->destroy();
			}
		}
	}
}

void Game::sEnemySpawner()
{
	if (m_currentFrame - m_lastEnemySpawnTime == m_enemyConfig.SI)
	{
		spawnEnemy();
	}
}

void Game::sRender()
{
	m_window.clear();

	for (auto& e : m_entities.getEntities())
	{
		e->cShape->circle.setPosition(e->cTransform->pos.x, e->cTransform->pos.y);

		e->cTransform->angle += 1.0f;
		e->cShape->circle.setRotation(e->cTransform->angle);

		m_window.draw(e->cShape->circle);
	}

	m_text.setString("Score: " + std::to_string(m_score));
	m_window.draw(m_text);

	m_window.display();
}

void Game::sUserInput()
{
	sf::Event event;
	while (m_window.pollEvent(event))
	{
		if (event.type == sf::Event::Closed)
		{
			m_running = false;
		}

		if (event.type == sf::Event::KeyPressed)
		{
			switch (event.key.code)
			{
			case sf::Keyboard::W:
				m_player->cInput->up = true;
				break;
			case sf::Keyboard::S:
				m_player->cInput->down = true;
				break;
			case sf::Keyboard::A:
				m_player->cInput->left = true;
				break;
			case sf::Keyboard::D:
				m_player->cInput->right = true;
				break;
			case sf::Keyboard::P:
				setPaused(!m_paused);
				break;
			default:break;
			}
		}

		if (event.type == sf::Event::KeyReleased)
		{
			switch (event.key.code)
			{
			case sf::Keyboard::W:
				m_player->cInput->up = false;
				break;
			case sf::Keyboard::S:
				m_player->cInput->down = false;
				break;
			case sf::Keyboard::A:
				m_player->cInput->left = false;
				break;
			case sf::Keyboard::D:
				m_player->cInput->right = false;
				break;
			case sf::Keyboard::Escape:
				m_window.close();
				break;
			default:
				break;
			}
		}

		if (event.type == sf::Event::MouseButtonPressed)
		{
			if (event.mouseButton.button == sf::Mouse::Left)
			{
				spawnBullets(m_player, Vec2(event.mouseButton.x, event.mouseButton.y));
			}

			if (event.mouseButton.button == sf::Mouse::Right)
			{
				spawnSpecialWeapon(m_player);
			}
		}
	}
}